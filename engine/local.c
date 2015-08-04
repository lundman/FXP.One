// $Id: local.c,v 1.15 2012/08/29 02:25:21 lundman Exp $
//
// Jorgen Lundman 14th March 2006.
//
// Deal with local (-to engine) transfers. We do this by spawning a new
// thread, with a pipe back to session.c. We then pretend to be a FTPD
// like normal. The only hooks required are for PASV, so we know which
// (if multihomed) interface IP we should reply as.
//
#if HAVE_CONFIG_H
#include <config.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifndef WIN32
#include <unistd.h>
#else
#define WINDOWS_MEAN_AND_LEAN
#include <winsock2.h>
#include <windows.h>
#include <ShlObj.h>
#include <Shlwapi.h>
#endif
#if HAVE_LIMITS_H
#include <limits.h>
#endif
#if HAVE_SYS_SYSLIMITS_H
#include <sys/syslimits.h>
#endif

#include "debug.h"
#include "lion.h"
#include "misc.h"
#include "parser.h"
#include "dirlist.h"

#define LOCAL_WITH_DECL
#include "local.h"



static int local_main(lion_t *, void *, void *);


//
// Spawn a new local handler. This is the only function called directly
// by engine (session). All following functions are running in own thread
// with own lion_poll() loops.
//
lion_t *local_spawn(void *user_data)
{
	lion_t *result;

	result = lion_fork(local_main, LION_FLAG_FULFILL, user_data, NULL);

	return result;

}






// *=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*
// *=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*
// *=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*
// *=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*
// *=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*
// *=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*
//
// Everything under this point runs in own thread, and own lion_poll()
// loop. Not to be confused by the process handling the main FXP.One
// engine loop.



enum {
	DATA_CMD_NONE = 0,
	DATA_CMD_LIST,
	DATA_CMD_STOR,
	DATA_CMD_APPE,
	DATA_CMD_RETR,
	DATA_CMD_WAITSSL
};

THREAD_SAFE static int       local_doexit    = 0;
THREAD_SAFE static int       mode            = 0;
THREAD_SAFE static int       prot            = 0;
THREAD_SAFE static int       ccsn            = 0;
THREAD_SAFE static lion_t   *data_handle     = NULL;
THREAD_SAFE static lion_t   *file_handle     = NULL;
THREAD_SAFE static char     *local_data_args = NULL;
THREAD_SAFE static int       local_data_ready= 0;
THREAD_SAFE static int       dirlist_started = 0;
THREAD_SAFE static int       data_cmd        = DATA_CMD_NONE;
THREAD_SAFE static lion64_t  rest            = 0;
THREAD_SAFE static unsigned long data_host   = 0;
THREAD_SAFE static int       data_port       = 0;
THREAD_SAFE static char     *local_rename_args= NULL;

#ifndef MAX_PATH
#ifdef PATH_MAX
#define MAX_PATH PATH_MAX
#endif
#endif

THREAD_SAFE static char      resolved[MAX_PATH] = "c:/";


struct cmds_struct {
	char name[20];    // Same max, but its static from .c
	void (*function)(lion_t *, char *, char *);
};

struct cmds_struct cmds[] = {
	{ "AUTH"   , local_cmd_auth },
	{ "USER"   , local_cmd_user },
	{ "PASS"   , local_cmd_pass },
	{ "SYST"   , local_cmd_syst },
	{ "CLNT"   , local_cmd_clnt },
	{ "FEAT"   , local_cmd_feat },
	{ "TYPE"   , local_cmd_type },
	{ "PBSZ"   , local_cmd_pbsz },
	{ "PROT"   , local_cmd_prot },
	{ "PASV"   , local_cmd_pasv },
	{ "PORT"   , local_cmd_port },
	{ "CWD"    , local_cmd_cwd  },
	{ "PWD"    , local_cmd_pwd  },
	{ "LIST"   , local_cmd_list },
	{ "REST"   , local_cmd_rest },
	{ "STOR"   , local_cmd_stor },
	{ "APPE"   , local_cmd_appe },
	{ "RETR"   , local_cmd_retr },
	{ "SIZE"   , local_cmd_size },
	{ "NOOP"   , local_cmd_noop },
	{ "CCSN"   , local_cmd_ccsn },
	{ "MDTM"   , local_cmd_mdtm },
	{ "MKD"    , local_cmd_mkd  },
	{ "RMD"    , local_cmd_rmd  },
	{ "QUIT"   , local_cmd_quit },
	{ "RNFR"   , local_cmd_rnfr },
	{ "RNTO"   , local_cmd_rnto },
	{ "DELE"   , local_cmd_dele },
	{ "ABOR"   , local_cmd_abor },
	{ "SITE"   , local_cmd_site },
	{ ""       , NULL           }
};

static void local_data(lion_t *engine);

int local_main(lion_t *engine, void *user_data, void *arg)
{

	debugf(" [local] main started...\n");

	lion_set_handler(engine, local_handler);

	// Spawn our dirlist helper.
	if (dirlist_init(1)) {
		lion_printf(engine, "500 Failed to spawn dirlist helper.\r\n");
		local_doexit = 1;
	}


	while(!local_doexit) {

		lion_poll(0, 5);

        //local_data(engine);

	}

	debugf(" [local] terminating.\n");
	dirlist_free();

	lion_exitchild(0);
	return(0); // just to ignore warning.
}





static void local_data(lion_t *engine)
{
	char *tmp;
	int waitssl = 0;

	local_data_ready++;

	debugf(" [local] local_data(%d)\n",
		   local_data_ready);

	// PORT mode, if 1, and port_host set, start connecting.
	if ((local_data_ready == 1) &&
		data_host &&
		data_port) {

		debugf(" [local] data connecting ... \n");
		data_handle = lion_connect(lion_ntoa(data_host),
								   data_port,
								   0,
								   0,
								   LION_FLAG_FULFILL,
								   engine);
		lion_set_handler(data_handle, local_data_handler);

		data_host = 0;
		data_port = 0;
		return;
	}

	if ((local_data_ready == 1) &&
		(data_cmd == DATA_CMD_WAITSSL) &&  // Actually RETR
		prot &&
		file_handle) {
		debugf(" [local] releasing RETR\n");
		lion_enable_read(file_handle);
		return;
	}

	// Only start when ready is == 2 (both LIST cmd, and data connected)
	if (local_data_ready != 2) {
		debugf(" [local] not quite ready\n");
		return;
	}

	switch(data_cmd) {

	case DATA_CMD_LIST:

		//dir = getcwd(NULL, 1024); // Solaris return NULL, if len is 0.
		if (!*resolved)
			strcpy(resolved, "c:/");

		debugf(" [local] calling dirlist .. '%s'\n", resolved);

#if 1
		dirlist_list(data_handle, resolved, NULL,
					 DIRLIST_PIPE | DIRLIST_USE_CRNL |
					 DIRLIST_LONG | DIRLIST_SHOW_DOT | DIRLIST_SHOW_DIRSIZE ,
					 engine);
#endif

		//SAFE_FREE(dir);

		break; // LIST



	case DATA_CMD_STOR:
#ifdef WIN32
		if (local_data_args[0] && local_data_args[1] == ':' && local_data_args[2] == '/') {
#else
		if (*local_data_args == '/') {
#endif
			debugf(" [local] starting STOR '%s'\n", local_data_args);

			file_handle = lion_open(local_data_args,
									O_WRONLY|O_CREAT,
									(mode_t) 0644,
									LION_FLAG_EXCLUSIVE | LION_FLAG_FULFILL,
									engine);
		} else {

			tmp = misc_strjoin(resolved,
							   local_data_args);
			debugf(" [local] starting STOR '%s'\n", tmp);

			file_handle = lion_open(tmp,
									O_WRONLY|O_CREAT,
									(mode_t) 0644,
									LION_FLAG_EXCLUSIVE | LION_FLAG_FULFILL,
									engine);
			SAFE_FREE(tmp);
		}

		lion_set_handler(file_handle, local_file_handler);
		lion_disable_read(file_handle);
		lion_enable_binary(file_handle);
		lion_enable_binary(data_handle);
		break; // STOR

	case DATA_CMD_APPE:

#ifdef WIN32
		if (local_data_args[0] && local_data_args[1] == ':' && local_data_args[2] == '/') {
#else
		if (*local_data_args == '/') {
#endif
			debugf(" [local] starting APPE '%s'\n", local_data_args);

			file_handle = lion_open(local_data_args,
									O_WRONLY|O_CREAT|O_APPEND,
									(mode_t) 0644,
									LION_FLAG_EXCLUSIVE | LION_FLAG_FULFILL,
									engine);
		} else {

			tmp = misc_strjoin(resolved,
							   local_data_args);
			debugf(" [local] starting APPE '%s'\n", tmp);

			file_handle = lion_open(tmp,
									O_WRONLY|O_CREAT|O_APPEND,
									(mode_t) 0644,
									LION_FLAG_EXCLUSIVE | LION_FLAG_FULFILL,
									engine);
			SAFE_FREE(tmp);
		}

		lion_set_handler(file_handle, local_file_handler);
		lion_disable_read(file_handle);
		lion_enable_binary(file_handle);
		lion_enable_binary(data_handle);

		break; // APPE

	case DATA_CMD_RETR:

#ifdef WIN32
		if (local_data_args[0] && local_data_args[1] == ':' && local_data_args[2] == '/') {
#else
		if (*local_data_args == '/') {
#endif
			debugf(" [local] starting RETR '%s'\n", local_data_args);

			file_handle = lion_open(local_data_args,
									O_RDONLY,
									(mode_t) 0644,
									/*LION_FLAG_EXCLUSIVE |*/ LION_FLAG_FULFILL,
									engine);
		} else {

			tmp = misc_strjoin(resolved,
							   local_data_args);
			debugf(" [local] starting RETR '%s'\n", tmp);

			file_handle = lion_open(tmp,
									O_RDONLY,
									(mode_t) 0644,
									/*LION_FLAG_EXCLUSIVE |*/ LION_FLAG_FULFILL,
									engine);
			SAFE_FREE(tmp);
		}

		lion_set_handler(file_handle, local_file_handler);
		if (prot && !lion_ssl_enabled(data_handle) ) {
			printf(" [local] file read disabled until SSL\n");
			lion_disable_read(file_handle);
			waitssl = 1;
			local_data_ready--;
		}
		lion_enable_binary(file_handle);
		lion_enable_binary(data_handle);

		break; // RETR

	case DATA_CMD_NONE:
		debugf(" [local] reached CMD_NONE!\n");
		break;

	} // switch


		debugf(" [local] finished\n");

	SAFE_FREE(local_data_args);
	local_data_ready = 0;

	if (waitssl)
		data_cmd = DATA_CMD_WAITSSL;
	else
		data_cmd = DATA_CMD_NONE;


}



















int local_handler(lion_t *handle, void *user_data,
				  int status, int size, char *line)
{
	int i;
	char *cmd;

	if (!user_data) {
		debugf(" [local] handler called with NULL session: %d\n", status);
		return -1;
	}



	switch(status) {

	case LION_PIPE_RUNNING:
		debugf(" [local] parent is connected.\n");
		lion_printf(handle, "220 FXP.One engine local transfer handler\r\n");
		break;

	case LION_PIPE_EXIT:
	case LION_PIPE_FAILED:
		debugf(" [local] parent lost (%s) -- terminating.\n",
			   line ? line : "");
		local_doexit = 1;
		break;

	case LION_CONNECTION_SECURE_ENABLED:
		debugf(" [local] engine connection upgraded to SSL.\n");
		break;
	case LION_CONNECTION_SECURE_FAILED:
		debugf(" [local] engine connection failed to SSL.\n");
		break;

	case LION_INPUT:
		debugf(" [local] parent '%s'\n", line);

		// Pull out the command:
		cmd = misc_digtoken(&line, " \r\n");
		if (!cmd) break;

		debugf(" [local] looking for '%s' '%s'\n", cmd, line);

		for (i = 0; cmds[i].function; i++) {
			if (!mystrccmp(cmds[i].name, cmd)) {
				cmds[i].function(handle, cmd, line);
				break;
			}
		}

		//lion_printf(handle, "500 Unknown Command '%s'\r\n", cmd);

		break;

	}


	return 0;
}





void local_cmd_auth(lion_t *engine, char *cmd, char *args)
{
	char *arg;

	debugf(" [local] received AUTH\n");

	arg = misc_digtoken(&args, " \r\n");
	if (!arg) {
		lion_printf(engine, "500 Command '%s' requires argument.\r\n", cmd);
		return;
	}

	if (mystrccmp(arg, "SSL") &&
		mystrccmp(arg, "TLS") &&
		mystrccmp(arg, "TLS-C")) {
		lion_printf(engine, "500 Unknown AUTH method '%s'.\r\n", arg);
		return;
	}


	//if (lion_ssl_set(engine, LION_SSL_SERVER) == 1) {
	//	lion_printf(engine, "234 Attempting %s connection\r\n", arg);
	//	return;
	//}

	// Since we talk via localhost, we do not bother to SSL, the
	// engine is made to think it is all SSLed internally.
	lion_printf(engine, "500 Unknown Command '%s'\r\n", cmd);

}




void local_cmd_user(lion_t *engine, char *cmd, char *args)
{

	debugf(" [local] received '%s'\n", cmd);
	lion_printf(engine, "331 Password required for %s.\r\n", args);

}


void local_cmd_pass(lion_t *engine, char *cmd, char *args)
{

	debugf(" [local] received '%s'\n", cmd);
	lion_printf(engine, "230 User logged in.\r\n");

}

void local_cmd_syst(lion_t *engine, char *cmd, char *args)
{

	debugf(" [local] received '%s'\n", cmd);
	lion_printf(engine, "215 UNIX Type: L8 Version: FXP.One\r\n");

}

void local_cmd_clnt(lion_t *engine, char *cmd, char *args)
{

	debugf(" [local] received '%s'\n", cmd);
	lion_printf(engine, "200 Noted.\r\n");

}

void local_cmd_feat(lion_t *engine, char *cmd, char *args)
{

	debugf(" [local] received '%s'\n", cmd);
	lion_printf(engine,
				"211-Features supported\r\n"
				" MDTM\r\n"
				" REST STREAM\r\n"
				" AUTH TLS\r\n"
				"211 End\r\n");

}

void local_cmd_type(lion_t *engine, char *cmd, char *args)
{
	char *arg;

	debugf(" [local] received '%s'\n", cmd);

	arg = misc_digtoken(&args, " \r\n");
	if (!arg) {
		lion_printf(engine, "500 Command '%s' requires argument.\r\n", cmd);
		return;
	}

	if (tolower(*arg) == 'i') mode = 1;
	else if (tolower(*arg) == 'a') mode = 0;
	else {
		lion_printf(engine, "500 Command '%s' requires valid arguments.\r\n", cmd);
		return;
	}

	lion_printf(engine, "200 Type set to %c.\r\n", *arg);

}

void local_cmd_pbsz(lion_t *engine, char *cmd, char *args)
{

	debugf(" [local] received '%s'\n", cmd);

	lion_printf(engine, "200 OK.\r\n");

}

void local_cmd_prot(lion_t *engine, char *cmd, char *args)
{
	char *arg;

	debugf(" [local] received '%s'\n", cmd);

	arg = misc_digtoken(&args, " \r\n");
	if (!arg) {
		lion_printf(engine, "500 Command '%s' requires argument.\r\n", cmd);
		return;
	}

	if (tolower(*arg) == 'p') prot = 1;
	else if (tolower(*arg) == 'c') prot = 0;
	else {
		lion_printf(engine, "500 Command '%s' requires valid arguments.\r\n", cmd);
		return;
	}

	lion_printf(engine, "200 OK\r\n");

}


void local_cmd_ccsn(lion_t *engine, char *cmd, char *args)
{
	char *arg;

	debugf(" [local] received '%s'\n", cmd);

	arg = misc_digtoken(&args, " \r\n");
	if (!arg) {
		lion_printf(engine, "200 %s Mode.\r\n",
					ccsn ? "Client" : "Server");
		return;
	}

	if (!mystrccmp("ON", arg)) ccsn = 1;
	else if (!mystrccmp("OFF", arg)) ccsn = 0;
	else {
		lion_printf(engine, "500 Command '%s' requires valid arguments.\r\n", cmd);
		return;
	}

	lion_printf(engine, "200 OK\r\n");

}





//
// We overload the PASV function to possibly send an IP along
// of the interface to use, as only the engine will know it.
// If not is sent, it is for dirlisting, so we bind to localhost.
//
void local_cmd_pasv(lion_t *engine, char *cmd, char *args)
{
	char *arg;
	int port;
	unsigned long addr;

	debugf(" [local] received '%s'\n", cmd);

	arg = misc_digtoken(&args, " \r\n");

	if (arg)
		addr = lion_addr(arg);
	else
		addr = lion_addr("127.0.0.1");


	local_data_ready = 0;
	data_host = 0;  // clear any prior PORT
	data_port = 0;

	// Open a listening socket
	port = 0;
	data_handle = lion_listen(&port,
							  0,
							  LION_FLAG_NONE, // Tell us now
							  engine);

	if (!data_handle) {
		lion_printf(engine, "425 can't build passive socket\r\n");
		return;
	}

	lion_set_handler(data_handle, local_data_handler);

	lion_printf(engine, "227 Entering Passive Mode (%s)\r\n",
				lion_ftp_port(addr, port));

}


void local_cmd_port(lion_t *engine, char *cmd, char *args)
{
	char *arg;

	debugf(" [local] received '%s'\n", cmd);

	arg = misc_digtoken(&args, " \r\n");

	if (!arg) {
		lion_printf(engine, "500 Command '%s' requires argument.\r\n", cmd);
		return;
	}

	if (!lion_ftp_pasv( arg, &data_host, &data_port )) {

		lion_printf(engine, "500 Command '%s' requires valid arguments.\r\n", cmd);
		return;
	}

	lion_printf(engine, "200 OK\r\n");

	// Some FTPD do not return on STOR command unless DATA is connected, so
	// we have to connect here
	local_data(engine);


}


void local_cmd_cwd(lion_t *engine, char *cmd, char *args)
{
	int isok = 1;
	char *tmp = NULL;

	debugf(" [local] received '%s'\n", cmd);

	if (!args || !*args) {
		lion_printf(engine, "500 Command '%s' requires valid arguments.\r\n", cmd);
		return;
	}

    // Windows chdir affect all threads, so we concat paths
#ifdef WIN32

	if (!args[0] || args[1] != ':' || args[2] != '/') {
	//if (1) {

#if 1
		tmp = misc_strjoin(resolved,
						   args);

		strncpy(resolved, tmp, sizeof(resolved)-1);
#endif
		if (!PathResolve(resolved,
						   NULL,
						   PRF_VERIFYEXISTS|PRF_REQUIREABSOLUTE))
						   isok = 2;
		SAFE_FREE(tmp);

	} else {

		strncpy(resolved, args, sizeof(resolved)-1);
		if (!PathResolve(resolved,
						   NULL,
						   PRF_VERIFYEXISTS|PRF_REQUIREABSOLUTE))
						   isok = 3;

	}

	//	GetLongPathName(resolved, resolved, sizeof(resolved));
		GetFullPathName(resolved, sizeof(resolved), resolved, NULL);
		{
			struct stat stbf;
			if (stat(resolved, &stbf)) {
				debugf(" [local] failed to stat '%s', returning failure\n", resolved);
				isok = 0;
			}
		}
#else

	if (args && *args != '/') {

		tmp = misc_strjoin(resolved,
						   args);
		debugf(" [local] trying combined '%s'\n", tmp);
		isok = realpath(tmp, resolved) ? 1 : 0;

		//strncpy(resolved, tmp, sizeof(resolved)-1);

		SAFE_FREE(tmp);

	} else {

		isok = realpath(args, resolved) ? 1 : 0;

	}

#endif
	debugf(" [local] CWD done. isok %d\n", isok);
	if (!isok) {
		lion_printf(engine, "550 CWD: %s\r\n",
					strerror(errno));
		return;
	}

    debugf(" [local] resolved to '%s'\n", resolved);
	lion_printf(engine, "250 CWD Command successful.\r\n");

}



void local_cmd_pwd(lion_t *engine, char *cmd, char *args)
{
	char *dir;

	debugf(" [local] received '%s'\n", cmd);


	dir = strdup(resolved);

#ifdef WIN32
	misc_undos_path(dir);
	misc_stripslash(dir);
#endif

	lion_printf(engine, "257 \"%s\" is current directory.\r\n",
				dir ? dir : "/");

	SAFE_FREE(dir);

}


void local_cmd_list(lion_t *engine, char *cmd, char *args)
{

	debugf(" [local] received '%s'\n", cmd);

	SAFE_COPY(local_data_args, args);

	data_cmd = DATA_CMD_LIST;

	local_data(engine);

	lion_printf(engine,
				"150 Opening %s%s mode data connection for /bin/ls.\r\n",
				mode ? "BINARY" : "ASCII",
				ccsn ? "+CCSN" : "");
}

void local_cmd_rest(lion_t *engine, char *cmd, char *args)
{

	debugf(" [local] received '%s'\n", cmd);

	if (!args || !*args) {
		lion_printf(engine, "500 Command '%s' requires valid arguments.\r\n", cmd);
		return;
	}

	rest = strtoull(args, NULL, 10);

	lion_printf(engine,
				"350 Restarting at %"PRIu64". Send STORE or RETRIEVE to initiate transfer.\r\n",
				rest);

}

void local_cmd_stor(lion_t *engine, char *cmd, char *args)
{

	debugf(" [local] received '%s'\n", cmd);

	if (!args || !*args) {
		lion_printf(engine, "500 Command '%s' requires valid arguments.\r\n", cmd);
		return;
	}

	SAFE_COPY(local_data_args, args);

	data_cmd = DATA_CMD_STOR;

	local_data(engine);

	lion_printf(engine,
				"150 Opening %s%s mode data connection for %s.\r\n",
				mode ? "BINARY" : "ASCII",
				ccsn ? "+CCSN" : "",
				args);
}

void local_cmd_appe(lion_t *engine, char *cmd, char *args)
{

	debugf(" [local] received '%s'\n", cmd);

	if (!args || !*args) {
		lion_printf(engine, "500 Command '%s' requires valid arguments.\r\n", cmd);
		return;
	}

	SAFE_COPY(local_data_args, args);

	data_cmd = DATA_CMD_APPE;

	local_data(engine);

	lion_printf(engine,
				"150 Opening %s%s mode data connection for %s.\r\n",
				mode ? "BINARY" : "ASCII",
				ccsn ? "+CCSN" : "",
				args);
}


void local_cmd_retr(lion_t *engine, char *cmd, char *args)
{

	debugf(" [local] received '%s'\n", cmd);

	if (!args || !*args) {
		lion_printf(engine, "500 Command '%s' requires valid arguments.\r\n", cmd);
		return;
	}

	SAFE_COPY(local_data_args, args);

	data_cmd = DATA_CMD_RETR;

	local_data(engine);

	lion_printf(engine,
				"150 Opening %s%s mode data connection for %s.\r\n",
				mode ? "BINARY" : "ASCII",
				ccsn ? "+CCSN" : "",
				args);
}


void local_cmd_size(lion_t *engine, char *cmd, char *args)
{
	struct stat sb;

	debugf(" [local] received '%s'\n", cmd);

	if (!args || !*args) {
		lion_printf(engine, "500 Command '%s' requires valid arguments.\r\n", cmd);
		return;
	}

	if (stat(args, &sb)) {
		lion_printf(engine, "550 %s: %s.\r\n", args, strerror(errno));
		return;
	}

	lion_printf(engine, "213 %"PRIu64"\r\n", (lion64_t) sb.st_size);

}

void local_cmd_mdtm(lion_t *engine, char *cmd, char *args)
{
	struct stat sb;
	struct tm *tt;

	debugf(" [local] received '%s'\n", cmd);

	if (!args || !*args) {
		lion_printf(engine, "500 Command '%s' requires valid arguments.\r\n", cmd);
		return;
	}

	if (stat(args, &sb)) {
		lion_printf(engine, "550 %s: %s.\r\n", args, strerror(errno));
		return;
	}

	if (!(sb.st_mode & S_IFREG)) {
		lion_printf(engine, "550 %s: not a plain file.\r\n", args);
		return;
	}

	/* Formulate the reply */
	tt = gmtime(&sb.st_mtime);
	lion_printf(engine, "213 "
				"%04d%02d%02d%02d%02d%02d\r\n",
				1900 + tt->tm_year,
				tt->tm_mon+1, tt->tm_mday,
				tt->tm_hour, tt->tm_min, tt->tm_sec);

}



void local_cmd_noop(lion_t *engine, char *cmd, char *args)
{

	debugf(" [local] received '%s'\n", cmd);

	lion_printf(engine, "200 NOOP Command Successful.\r\n");

}


void local_cmd_mkd(lion_t *engine, char *cmd, char *args)
{

	debugf(" [local] received '%s'\n", cmd);

	if (!args || !*args) {
		lion_printf(engine, "500 Command '%s' requires valid arguments.\r\n", cmd);
		return;
	}

	if (mkdir(args, (mode_t) 0755)) {
		lion_printf(engine, "550 %s: %s.\r\n", args, strerror(errno));
		return;
	}

	lion_printf(engine, "257 MKD Command successful.\r\n");

}



void local_cmd_rmd(lion_t *engine, char *cmd, char *args)
{

	debugf(" [local] received '%s'\n", cmd);

	if (!args || !*args) {
		lion_printf(engine, "500 Command '%s' requires valid arguments.\r\n", cmd);
		return;
	}

	if (rmdir(args)) {
		lion_printf(engine, "550 %s: %s.\r\n", args, strerror(errno));
		return;
	}

	lion_printf(engine, "257 RMD command successful.\r\n");

}



void local_cmd_quit(lion_t *engine, char *cmd, char *args)
{

	debugf(" [local] received '%s'\n", cmd);

	lion_printf(engine, "221 Goodbye.\r\n");

	local_doexit = 1;

}




void local_cmd_rnfr(lion_t *engine, char *cmd, char *args)
{
	struct stat sb;

	debugf(" [local] received '%s'\n", cmd);

	if (!args || !*args) {
		lion_printf(engine, "500 Command '%s' requires valid arguments.\r\n", cmd);
		return;
	}

	if (stat(args, &sb)) {
		lion_printf(engine, "550 %s: %s.\r\n", args, strerror(errno));
		return;
	}

	SAFE_COPY(local_rename_args, args);

	lion_printf(engine, "350 File exists, ready for destination name\r\n");

}


void local_cmd_rnto(lion_t *engine, char *cmd, char *args)
{

	debugf(" [local] received '%s'\n", cmd);

	if (!args || !*args) {
		lion_printf(engine, "500 Command '%s' requires valid arguments.\r\n", cmd);
		return;
	}

	if (!local_rename_args) {
		lion_printf(engine, "550 Received RNTO without a previous RNFR!\r\n");
		return;
	}

	if (rename(local_rename_args, args))
		lion_printf(engine, "550 %s: %s.\r\n", args, strerror(errno));
	else
		lion_printf(engine, "250 RNTO command successful.\r\n");

	SAFE_FREE(local_rename_args);
}



void local_cmd_dele(lion_t *engine, char *cmd, char *args)
{

	debugf(" [local] received '%s'\n", cmd);

	if (!args || !*args) {
		lion_printf(engine, "500 Command '%s' requires valid arguments.\r\n", cmd);
		return;
	}

	if (unlink(args)) {
		lion_printf(engine, "550 %s: %s.\r\n", args, strerror(errno));
		return;
	}

	lion_printf(engine, "257 DELE command successful.\r\n");

}


void local_cmd_abor(lion_t *engine, char *cmd, char *args)
{

	debugf(" [local] received '%s'\n", cmd);

	if (data_handle) {
		// Stop 226 msgs
		lion_set_userdata(data_handle, NULL);
		// Close data transfer
		lion_close(data_handle);
	}

	lion_printf(engine, "225 ABOR command successful.\r\n");

}


void local_cmd_site(lion_t *engine, char *cmd, char *args)
{

	debugf(" [local] received '%s'\n", cmd);

	lion_printf(engine, "200-SITE command to local site.\r\n");
	lion_printf(engine, "200 OK\r\n");

}






























int local_data_handler(lion_t *handle, void *user_data,
					   int status, int size, char *line)
{

	switch(status) {

	case LION_CONNECTION_CONNECTED:
		if (handle == data_handle) {

			if (prot) {
				int ssl;
				debugf(" [local] connected, requesting SSL\n");
				ssl = lion_ssl_set(data_handle,
							 LION_SSL_CLIENT);
				debugf(" [local] SSL said %d\n", ssl);
				return 0;
			}

			debugf(" [local] DATA connected\n");
			local_data(user_data);
		}
		break;

	case LION_CONNECTION_NEW:
		debugf(" [local] incoming connection\n");
		data_handle = lion_accept(handle, 1, LION_FLAG_FULFILL,
								  user_data, NULL, NULL);
		lion_set_handler(data_handle, local_data_handler);
		break;

	case LION_CONNECTION_CLOSED:
		if (user_data)
			lion_printf(user_data, "226 Transfer complete.\r\n");

		data_handle = NULL;
		if (file_handle) lion_close(file_handle);
		break;

	case LION_CONNECTION_LOST:
		if (user_data)
			lion_printf(user_data, "550 Transfer failed: %s.\r\n", line);

		data_handle = NULL;
		if (file_handle) lion_close(file_handle);
		break;

	case LION_CONNECTION_SECURE_ENABLED:
		debugf(" [local] data connection upgraded to SSL.\n");
		local_data(user_data);
		break;

	case LION_CONNECTION_SECURE_FAILED:
		debugf(" [local] data connection failed to SSL.\n");
		break;

	case LION_INPUT:
		// If input is ":int "
		if (*line == ':')
			debugf(" [local] '%s'\n", line);

		if (!mystrccmp(":END", line)) {

			debugf(" [local] END seen. Closing all\n");

			dirlist_started = 0;
			lion_close(data_handle);
			data_handle = NULL;
			break;
		}

		if ((*line == ':') &&
			atoi(&line[1]) == 0) {
			dirlist_started = 1;
			break;
		}

		if ((*line == ':') &&
			atoi(&line[1]) > 0) {
			if (user_data)
				lion_printf(user_data, "550 LIST failed: %s\r\n", line);
			dirlist_started = 0;
			lion_close(data_handle);
			data_handle = NULL;
		}

		if (dirlist_started)
			lion_printf(data_handle, "%s\r\n", line);
		break;

	case LION_BUFFER_USED:
		if (file_handle)
			lion_disable_read(file_handle);
		break;

	case LION_BUFFER_EMPTY:
		if (file_handle)
			lion_enable_read(file_handle);
		break;

	case LION_BINARY:
		if (file_handle)
			lion_output(file_handle, line, size);

	}


	return 0;
}


int local_file_handler(lion_t *handle, void *user_data,
					   int status, int size, char *line)
{

	switch(status) {

	case LION_FILE_OPEN:
		debugf(" [local] file open\n");
		if (rest)
			lseek(lion_fileno(handle),
				  (off_t)rest,
				  SEEK_SET);
		rest = 0;
		break;
	case LION_FILE_CLOSED:
		debugf(" [local] file closed\n");
		file_handle = NULL;
		if (data_handle)
			lion_close(data_handle);
		break;
	case LION_FILE_FAILED:
		debugf(" [local] file failed: %s\n", line);
		if (data_handle) {
			// Setting userdata to NULL stops data from sending 550 msg
			lion_set_userdata(data_handle, NULL);
			lion_close(data_handle);
			if (user_data)
				lion_printf(user_data,
							"550 %s: Can't open for %s: %s\r\n",
							data_cmd == DATA_CMD_RETR ? "reading" : "writing",
							line);
		}
		file_handle = NULL;
		break;

	case LION_BUFFER_USED:
		if (data_handle)
			lion_disable_read(data_handle);
		break;

	case LION_BUFFER_EMPTY:
		if (data_handle)
			lion_enable_read(data_handle);
		break;

	case LION_BINARY:
		if (data_handle)
			lion_output(data_handle, line, size);
		break;
	}


	return 0;
}
