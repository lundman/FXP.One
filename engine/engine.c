// $Id: engine.c,v 1.31 2012/05/11 22:45:20 lundman Exp $
//
// Jorgen Lundman 10th October 2003.
//
// FXP.One main server code. This is the top/starting point for the server.
//

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#ifndef WIN32
#include <pwd.h>
#include <getopt.h>
#include <unistd.h>
#endif

#include <sys/types.h>
#include <sys/stat.h>

#include <errno.h>

#include "lion.h"
#include "misc.h"

#include "engine.h"
#include "settings.h"
#include "users.h"
#include "command.h"
#include "sites.h"
#include "manager.h"
#include "queue.h"
#include "http.h"
#include "debug.h"
#include "version.h"

#ifdef WITH_SSL
#include <openssl/opensslv.h>
#endif

#ifdef WIN32
#include <ShlObj.h>
extern int getopt();
extern char *optarg;
extern int optind;
#endif


__RCSID("$FXPOne: engine.c,v 1.3 2003/10/10 06:06:08 lundman Exp $");


static int skip_password = 0;
static int use_cwd = 0;
static int save_allconf = 0;
static int disable_websockets = 0;

// While this is true the programs keeps running.
int engine_running = 1;

int engine_nodelay = 0;

int engine_foreground = 0;  // Don't fork.

int engine_firstrun = 0;


RETSIGTYPE exit_interrupt(void)
{

	engine_running = 0;

}


void fail(char *msg)
{

	puts(msg);

	exit(-1);

}

int lion_userinput(lion_t *handle, void *user_data,
                                   int status, int size, char *line)
{

	printf("engine: default userinput called! %d,%d:%s\n",
		status, size, line ? line : "(null)");

	return 0;
}


#ifdef WIN32
/* Windows implementation by Martin Lambers <address@hidden>,
   improved by Simon Josefsson. */

/* For PASS_MAX. */
#include <limits.h>

#ifndef PASS_MAX
# define PASS_MAX 512
#endif

char *
getpass (const char *prompt)
{
  char getpassbuf[PASS_MAX + 1];
  size_t i = 0;
  int c;

  if (prompt)
    {
      fputs (prompt, stderr);
      fflush (stderr);
    }

  for (;;)
    {
      c = _getch ();
      if (c == '\r')
       {
         getpassbuf[i] = '\0';
         break;
       }
      else if (i < PASS_MAX)
       {
         getpassbuf[i++] = c;
       }

      if (i >= PASS_MAX)
       {
         getpassbuf[i] = '\0';
         break;
       }
    }

  if (prompt)
    {
      fputs ("\r\n", stderr);
      fflush (stderr);
    }

  return strdup (getpassbuf);
}
#endif



int main(int argc, char **argv)
{
	int pre_argc;
	char **pre_argv;
    struct stat stbf;

	signal(SIGINT, exit_interrupt);
#ifndef WIN32
	signal(SIGHUP, exit_interrupt);
#endif

	printf("FXP.One server engine.\n");

	printf("Version %u.%u Protocol %u.%u build %u (%s@%s) - %s\n\n",
		   VERSION_MAJOR,
		   VERSION_MINOR,
		   PROTOCOL_MAJOR,
		   PROTOCOL_MINOR,
		   VERSION_BUILD,
		   VERSION_USER,
		   VERSION_HOST,
		   VERSION_DATE);

#ifdef WITH_SSL
    printf("SSL Headers version %s.\n",
           OPENSSL_VERSION_TEXT);
#endif

#ifdef DEBUG
	//lion_trace_file("trace.log");
#endif


	// Parse the arguments first here, this controls if we SHOULD CWD to
	// read the conf file, and indeed, maybe even the name of the conf file.
	pre_argc = argc;
	pre_argv = argv;
	arguments(pre_argc, pre_argv);


	engine_workdir();

    if (!engine_running) {
#ifdef WIN32
		getch();
#endif
		exit(0);
	}
	//We just use default. Since we don't transfer data, a small buffer
	//is actually just fine here.
	lion_buffersize(16384);

	//lion_ssl_ciphers("SSLv2:-LOW:-EXPORT:RC4+RSA");
	if (lion_init()) fail("engine: lion failed to initialise\n");


	// Init/Read settings,
	settings_init();


	// Since we are using lion to read these files, we need the poll
	// function called until we are done. So we have a mini main loop here
	// for reading the initial state options and settings. (Like the port
	// number to listen on).
	while(settings_initialising) {
		lion_poll(0,1);
	}

	// Really parse the arguments. settings file read, now we can over^ride
	// those settings with argument settings.
	arguments(argc, argv);


	// Read pass from stdin?
	if (skip_password == 2) {
		char pass[1024];

		fgets(pass, sizeof(pass), stdin);
		misc_strip(pass);

		users_setkey(pass);
		sites_setkey(pass);

		memset(pass, 0, sizeof(pass));

		printf("Received secret.\n");
	}

	fflush(stdout);

    if (stat(".FXP.One.users", &stbf)) {
        engine_firstrun = 1;
    }

	if (!skip_password) {
		// Ask user for password
		char *pass;

        if (engine_firstrun) {
            printf("\nFXP.Oned uses a 'key' to encrypt all local data files.\n");
            printf("Please create a new key by entering it now:\n");
        }
		pass = getpass("Key: ");
		users_setkey(pass);
		sites_setkey(pass);

		memset(pass, 0, strlen(pass));

	}


	// Init/Read user DB
	users_init();

	// Init/Read sites
	sites_init();

	// Init/Read queues if any

	manager_init();


	queue_init();

	// Init listening socket
	command_init();

    // WebSockets port
    if (!disable_websockets)
        http_init();

	// save all conf? (See engine_workdir)
	if (save_allconf) {

		// Make sure that we have finished loading the conf files.
		while(!sites_iodone() || !users_iodone())
			lion_poll(0, engine_nodelay ? 0 : 1);

		save_allconf = 0;
		engine_workdir(); // to CWD

		settings_save();
		users_save();
		sites_save();

	}



	// Decide if we should background.
	if (engine_running && !engine_foreground) {
		engine_daemonise();
	} else {
        printf("Ready (not backgrounding)\n");
    }


	//
	// We allow the state processing to set "nodelay", and we ensure we
	// call it again as soon as possible.
	while (engine_running) {

	lion_poll(0, engine_nodelay ? 0 : 1);

		// Call queue runner. Keep calling it until nodelay is not set.
		// nodelay means we are just changing state.
		do {
			engine_nodelay = 0;
			queue_run();
		} while (engine_nodelay);

		manager_cleanup();

	}

	printf("\nShutting down...\n");


	// Release listening sockets
    if (!disable_websockets)
        http_free();

	command_free();

	queue_free();

	manager_free();

	users_free();
	// Save queues.

	sites_free();


	lion_free();

#ifdef WIN32
	Sleep(1000);
#endif
	return 0;
}







void options(char *prog)
{

	printf("\n");
	printf("%s - FXP.One server engine.\n", prog);
	printf("%s [-h] [...]\n\n", prog);
	printf("  options:\n");

	printf("  -h          : display usage help (this output)\n");
	printf("  -N          : start with no Key set (insecure!)\n");
	printf("  -P <key>    : start with given Key  (insecure!)\n");
	printf("  -I          : read Key from stdin.\n");
	printf("  -p <port>   : listen on alternative port (default 8885) [1]\n");
	printf("  -w <port>   : WebSocket alternative port (default 8886) [1]\n");
	printf("  -W          : Disable WebSocket port\n");
	printf("  -S          : Enforce SSL only connections [1]\n");
	printf("  -s          : Disable SSL only restriction [1]\n");
	printf("  -f          : foreground, do not fork.\n");
	printf("  -d          : verbose debug output.\n");
	printf("  -D          : Do not CWD to default, work from PWD.\n");
    printf("  -C <cipher> : Specify SSL Cipher list\n");
	printf("\n[1] This overrides the value in the settings file.\n");
	printf("\nThe Key (if given) is used to encrypt all the files that FXP.One\n");
	printf("stores on local disk. You need to specify it the first time, when it creates\n");
	printf("the initial files to work. (Or delete the dot-files and start again)\n");

	printf("\n\n(c) Jorgen Lundman <lundman@lundman.net>\n\n");

	exit(0);

}





void arguments(int argc, char **argv)
{
	int opt;

	while ((opt=getopt(argc, argv,
					   "hP:Np:IfdDC:w:W")) != -1) {

		switch(opt) {

		case 'h':
			options(argv[0]);
			break;

		case 'P':
			users_setkey(optarg);
			sites_setkey(optarg);
			skip_password = 1;
			break;

		case 'N':
			users_setkey(NULL);
			sites_setkey(NULL);
			skip_password = 1;
			break;

		case 'I':
			skip_password = 2;
			break;

		case 'D':
			use_cwd = 1;
			break;

		case 'p':
			settings_values.command_port = atoi(optarg);
			break;

		case 'w':
			settings_values.http_port = atoi(optarg);
			break;

		case 'W':
			disable_websockets = 1;
			break;

		case 'S':
			settings_values.command_ssl_only = YNA_YES;
			break;

		case 's':
			settings_values.command_ssl_only = YNA_NO;
			break;

		case 'f':
			engine_foreground = 2;
			break;

		case 'd':
			debug_on++;
			break;

		case 'C':
            lion_ssl_ciphers(optarg);
			break;

		default:
			printf("Unknown option.\n");
			break;
		}
	}

	argc -= optind;
	argv += optind;

	// argc and argv adjusted here.

}





void engine_daemonise(void)
{
	FILE *i;

	printf("Backgrounded...\r\n");

#ifndef WIN32
	if (fork()) {
		exit(0);
	}

	setsid();
#endif

	fflush(stdout);

	fclose(stdin);
	fclose(stdout);
	fclose(stderr);

	i = fopen("/dev/null", "a");
	if (!i) return;

	dup2(fileno(i), 0);
    dup2(fileno(i), 1);
    dup2(fileno(i), 2);

	fclose(i);

}


#if defined (__APPLE__) || defined (PLATFORM_OSX11)
#define DEFAULT_PATH "%s/Library/Application Support/FXP.One"
#endif

#ifdef WIN32
#define DEFAULT_PATH "%s/FXP.One"
#endif

#ifndef DEFAULT_PATH
#define DEFAULT_PATH "%s/.FXP.One"
#endif


void engine_workdir()
{
	char build_path[1024];
	char *homedir, *tmp;
	struct stat stsb;
#ifdef WIN32
	char path[MAX_PATH];
#endif

	// Find our homedir, either from $HOME or sumthin
	homedir = getenv("HOME");
	if (!homedir) {
#ifndef WIN32
		struct passwd *p;
		p = getpwuid(getuid());
		if (p) homedir = p->pw_dir;
		else
			homedir = "/tmp/";
#else
		if (SHGetFolderPath(NULL, CSIDL_PERSONAL|CSIDL_FLAG_CREATE,NULL,0,path) == S_OK) {
			homedir = path;
		} else {
			homedir = "./";
		}
#endif
	}

	snprintf(build_path, sizeof(build_path), DEFAULT_PATH, homedir);

    printf("workdir: %s\n", build_path);

	// Check it exists?
	if (!stat(build_path, &stsb)) {

		// CWD there, unless -D was specified.
		if (use_cwd) return;

#ifdef WFXPROOTDIR
        // Is wfxp installed?
        tmp = misc_strjoin(build_path, HTTP_DIR);
        if (stat(tmp, &stsb)) {
            // Are the source files present?
            if (!stat(WFXPROOTDIR, &stsb)) {
                http_install_wfxp(WFXPROOTDIR, tmp);
            } else if (!stat("../clients/wfxp/", &stsb)) {
                // What is not installed, but running in src engine dir?
                http_install_wfxp("../clients/wfxp/", tmp);
            }
        }
        SAFE_FREE(tmp);
#endif

		if (!chdir(build_path)) return;

		printf("warning: failed to chdir to default directory '%s': %s\n",
			   build_path, strerror(errno));

		return;
	}

	// Directory does not exist.
	if (mkdir(build_path, 0700)) {
		printf("warning: failed to create default directory '%s': %s\n",
			   build_path, strerror(errno));
		return;
	}

	printf("Created default directory '%s' for data files.\n", build_path);

	// Are there existing file in PWD, which was the old way to run FXP.One?
	if (!stat(SITES_CONF, &stsb)) {
		printf("Copying existing conf files from CWD to default directory.\n");
		printf("If you prefer the old way of running FXP.Oned, ie from CWD,\n");
		printf("you can do so by specifying the '-D' option.\n");
		printf("You might want to remove the 3 data files from this directory.\n\n");
		save_allconf = 1;
		// Do not CWD here, but return. Then, once we have read all the files
		// we will call this function again, and since the default directory
		// now exists (mkdir was OK after all) it WILL CWD then. Then we
	    // save all the conf files due to the save_allconf setting.
		return;
	}

    // Check for certificate
    tmp = misc_strjoin(build_path, CERTFILE);
    if (stat(tmp, &stsb)) {
        printf("\nFXP.Oned require an SSL certificate to support encrypted engine connections.\n");
        printf("If you do not have a certificate, you can make one running this command:\n\n");
        printf("openssl req -new -x509 -days 365 -nodes -out \"%s\" -keyout \"%s\"\n\n",
               tmp, tmp);

        // We quit the engine, as to be less confusing for new users.
        engine_running = 0;

    }
    SAFE_FREE(tmp);

	// All done.
	if (chdir(build_path)) {
		printf("warning: failed to CWD to default directory '%s': %s\n",
			   build_path, strerror(errno));
		return;
	}


}
