#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef WIN32
#include <unistd.h>
#include <sys/time.h>
#endif

#include "lion.h"
#include "misc.h"

#include "conf.h"
#include "file.h"
#include "parser.h"
#include "site.h"
#include "clomps.h"
#include "debug.h"


unsigned int num_sites = 0;
unsigned int num_done  = 0;
unsigned int num_sitelist = 0;


site_t **sites = NULL;
file_t **new_files = NULL;
unsigned int num_new = 0;



static parser_command_list_t site_command_list[] = {
        { COMMAND( "WELCOME" ),    site_cmd_welcome     },
        { COMMAND( "SSL" ),        site_cmd_ssl         },
        { COMMAND( "AUTH" ),       site_cmd_auth        },
        { COMMAND( "SITELIST" ),   site_cmd_sitelist    },
        { COMMAND( "SESSIONNEW" ), site_cmd_sessionnew  },
        { COMMAND( "DISCONNECT" ), site_cmd_disconnect  },
        { COMMAND( "CONNECT" ),    site_cmd_connect     },
        { COMMAND( "CWD" ),        site_cmd_cwd         },
        { COMMAND( "DIRLIST" ),    site_cmd_dirlist     },
        { NULL,  0         ,                    NULL }
};








void send_auth(fxpone_t *fxpone)
{

	lion_printf(fxpone->handle, "AUTH|USER=%s|PASS=%s\r\n",
				fxpone->user,
				fxpone->pass);

}






int site_handler(lion_t *handle, void *user_data,
				 int status, int size, char *line)
{
	fxpone_t *fxpone = (fxpone_t *)user_data;

	switch(status) {

	case LION_CONNECTION_CONNECTED:
		debugf("connected to FXP.One\n");
        connected = 1;
		break;

	case LION_CONNECTION_LOST:
		printf("Failed to connect to FXP.One: %d\n", size);
		//do_exit=1;
        connected = 0;
		if (fxpone)
			fxpone->handle = NULL;
		break;

	case LION_CONNECTION_CLOSED:
		printf("Connection closed to FXP.One\n");
        // We attempt reconnects with clomps-irc
		//do_exit=1;
        connected = 0;
		if (fxpone)
			fxpone->handle = NULL;
		break;

	case LION_CONNECTION_SECURE_ENABLED:
		debugf("successfully negotiated SSL\n");
		send_auth(fxpone);
		break;

	case LION_CONNECTION_SECURE_FAILED:
		printf("SSL negotiation failed\n");
		do_exit=1;
		if (fxpone)
			fxpone->handle = NULL;
		break;

	case LION_INPUT:
		//debugf("FXP.One: %s\n", line);
		parser_command(site_command_list, line, (void *)fxpone);
	}

	return 0;

}









// WELCOME|name=FXP.Oned|version=0.1|build=359|protocol=1.2|SSL=optional
void site_cmd_welcome(char **keys, char **values, int items,void *optarg)
{
	fxpone_t *the_engine = optarg;
	char *ssl;

	ssl = parser_findkey(keys, values, items, "SSL");

	if (ssl && !mystrccmp("disabled", ssl)) {
		send_auth(the_engine);
		return;
	}

	lion_printf(the_engine->handle, "SSL\r\n");
}


// SSL|CODE=0|OK|Msg=Attempting SSL negotiations.
void site_cmd_ssl(char **keys, char **values, int items,void *optarg)
{
	fxpone_t *the_engine = optarg;
	char *code;

	code = parser_findkey(keys, values, items, "CODE");

	if (!code || atoi(code) != 0) {
		printf("SSL support failed with FXP.One\n");
		do_exit = 1;
		return;
	}

	lion_ssl_set(the_engine->handle, LION_SSL_CLIENT);

}


// >> AUTH|OK|MSG=Successful
void site_cmd_auth(char **keys, char **values, int items,void *optarg)
{
	fxpone_t *the_engine = optarg;
	char *code;

	code = parser_findkey(keys, values, items, "CODE");

	// We didn't get code, or, code is non-zero
	if (!code || atoi(code)) {
		printf("Failed to authenticate with FXP.One\n");
		do_exit = 1;
		return;
	}

	debugf("Successfully connected to FXP.One\n");

	lion_printf(the_engine->handle, "SITELIST\r\n");
}




// >> SITELIST|SITEID=1|NAME=localhost|HOST=127.0.0.1|PORT=21|USER=mp3|PASS=mp3|PASSIVE=1|FXP_PASSIVE=2|CONTROL_TLS=2|DATA_TLS=2|optional_variable=roger moore
void site_cmd_sitelist(char **keys, char **values, int items,void *optarg)
{
	fxpone_t *the_engine = optarg;
	char *name, *siteid, *end;
	char *dskiplist, *dpasslist, *fskiplist, *fpasslist, *fskipempty;
	int i;

	end    = parser_findkey(keys, values, items, "END");
	name   = parser_findkey(keys, values, items, "NAME");
	siteid = parser_findkey(keys, values, items, "SITEID");

	if (end) {

		// num_sitelist has how many sites we found and connected to
		// num_sites is how many we are waiting for before processing.
		num_sites = num_sitelist;
		return;

	}




	if (!name || !siteid) return;

	dskiplist = parser_findkey(keys, values, items, "DSKIPLIST");
	dpasslist = parser_findkey(keys, values, items, "DPASSLIST");
	fskiplist = parser_findkey(keys, values, items, "FSKIPLIST");
	fpasslist = parser_findkey(keys, values, items, "FPASSLIST");
	fskipempty = parser_findkey(keys, values, items, "FSKIPEMPTY");


	for (i = 0; i < num_sites; i++) {

		if (!sites[i]) continue;

		if (!mystrccmp(sites[i]->name, name)) {

			sites[i]->siteid = atoi(siteid);

			SAFE_COPY(sites[i]->dskiplist, dskiplist);
			SAFE_COPY(sites[i]->dpasslist, dpasslist);
			SAFE_COPY(sites[i]->fskiplist, fskiplist);
			SAFE_COPY(sites[i]->fpasslist, fpasslist);

			if (fskipempty && (atoi(fskipempty) == 0))
				sites[i]->fskipempty = 0;
			else
				sites[i]->fskipempty = 1;

			num_sitelist++;

            // IRC sets skip, so we dont do initial login
            if (!sites[i]->skip)
                lion_printf(the_engine->handle, "SESSIONNEW|SITEID=%u\r\n",
                            atoi(siteid));

		}

	}

}




// << SESSIONNEW|SITEID=0|SID=1
void site_cmd_sessionnew(char **keys, char **values, int items,void *optarg)
{
	fxpone_t *the_engine = optarg;
	char *siteid, *sid;
	int i;

	sid    = parser_findkey(keys, values, items, "SID");
	siteid = parser_findkey(keys, values, items, "SITEID");

	if (!siteid) return;

	for (i = 0; i < num_sites; i++) {

		if (!sites[i]) continue;

		if (sites[i]->siteid == atoi(siteid)) {

			if (!sid) {
				printf("Site failed: %s\n", sites[i]->name);
				sites[i]->failed = 1;
				site_ready(the_engine->handle, sites[i]);
				return;
			}

			sites[i]->sid = atoi(sid);

		}

	}

}


// << DISCONNECT|SID=1|MSG=Undefined error: 0
void site_cmd_disconnect(char **keys, char **values, int items,void *optarg)
{
	char *sid;
	int i;
	fxpone_t *the_engine = optarg;

	sid    = parser_findkey(keys, values, items, "SID");

	if (!sid) return;

	for (i = 0; i < num_sites; i++) {

		if (sites[i] && (sites[i]->sid == atoi(sid))) {

			printf("Site %s disconnected\n", sites[i]->name);

			sites[i]->failed = 1;
			site_ready(the_engine->handle, sites[i]);

		}

	}

}


// << CONNECT|SID=1
void site_cmd_connect(char **keys, char **values, int items,void *optarg)
{
	fxpone_t *the_engine = optarg;
	char *sid;
	int i;

	sid    = parser_findkey(keys, values, items, "SID");

	if (!sid) return;

	for (i = 0; i < num_sites; i++) {

		if (sites[i] && (sites[i]->sid == atoi(sid))) {

			printf("Site %s processing dir '%s'\n", sites[i]->name,
				   sites[i]->dirs[sites[i]->current_dir]);

			lion_printf(the_engine->handle, "CWD|SID=%u|PATH=%s\r\n",
						sites[i]->sid,
						sites[i]->dirs[sites[i]->current_dir]);

		}

	}

}




// << CWD|SID=2|REPLY=250|OK|MSG=250 CWD command successful.
void site_cmd_cwd(char **keys, char **values, int items,void *optarg)
{
	fxpone_t *the_engine = optarg;
	char *code, *sid, *msg;
	int i;
	unsigned int id;

	sid    = parser_findkey(keys, values, items, "SID");
	code   = parser_findkey(keys, values, items, "CODE");
	msg    = parser_findkey(keys, values, items, "MSG");

	if (!sid) return;

	id = atoi(sid);

	for (i = 0; i < num_sites; i++) {

		if (!sites[i]) continue;

		if (sites[i]->sid == id) {

			if (code && atoi(code) == 0) {
				lion_printf(the_engine->handle, "DIRLIST|SID=%u\r\n",
							sites[i]->sid);
			} else {

				printf("Site %s incorrect path '%s' : %s\n",
					   sites[i]->name,
					   sites[i]->dirs[ sites[i]->current_dir],
					   msg ? msg : "");

				sites[i]->failed = 1;
				lion_printf(the_engine->handle, "DISCONNECT|SID=%u\r\n",
							sites[i]->sid);
			}
		}

	}

}





//<< DIRLIST|SID=1|FID=0|NAME=giana_sounds|DATE=1057590000|SIZE=512|USER=nobody|GROUP=nobody|PERM=drwxrwxrwx|type=directory
void site_cmd_dirlist(char **keys, char **values, int items,void *optarg)
{
	fxpone_t *the_engine = optarg;
	char *sid, *name, *fid, *date, *size, *type, *end, *begin;
	int i;
	unsigned int id;
	site_t *site;
	file_t *file, **tmp;

	sid    = parser_findkey(keys, values, items, "SID");
	name   = parser_findkey(keys, values, items, "NAME");
	fid    = parser_findkey(keys, values, items, "FID");
	date   = parser_findkey(keys, values, items, "DATE");
	size   = parser_findkey(keys, values, items, "SIZE");
	type   = parser_findkey(keys, values, items, "FTYPE");
	begin  = parser_findkey(keys, values, items, "BEGIN");
	end    = parser_findkey(keys, values, items, "END");

	if (!sid) return;

	if (begin) return;

	id = atoi(sid);

	for (i = 0; i < num_sites; i++) {

		if (!sites[i]) continue;

		if (sites[i]->sid == id) {

			site = sites[i];

			if (end) {

				site->current_dir++;
				if (site->current_dir >= site->num_dirs) {
					site_ready(the_engine->handle, site);
					return;
				}

				printf("Site %s processing dir '%s'\n", sites[i]->name,
					   sites[i]->dirs[sites[i]->current_dir]);

				lion_printf(the_engine->handle, "CWD|SID=%u|PATH=%s\r\n",
							sites[i]->sid,
							sites[i]->dirs[sites[i]->current_dir]);

				return;

			}

			//			SAFE_COPY(sites[i]->name, name);
			file = file_new();
			if (!file) return;

			tmp = realloc(site->files, sizeof(file_t *)*(site->num_files + 1));
			if (!tmp) {
				file_free(file);
				return;
			}

			site->files = tmp;

			site->files[ site->num_files ] = file;
			site->num_files++;

			SAFE_COPY(file->name, name);
			file->date = strtoul(date, NULL, 10);
			file->size = strtoull(size, NULL, 10);
			if (type && !mystrccmp("directory", type))
				file->type = 1;

			// Remember the source path we came from
			file->current_dir = site->current_dir;

		}

	}

}



void site_ready(lion_t *engine, site_t *site)
{


	lion_printf(engine,"SESSIONFREE|SID=%u\r\n",
				site->sid);

	site->sid = 0;

	num_done++;

	printf("%12s finished processing: %4u files (%3d/%3d)\n", site->name,
		   site->num_files,
		   num_done, num_sites);

}








void sites_compare(void)
{
	int i,j;
	site_t *site;
	file_t *file, **tmp;

	// Sort it
	for (i = 0; i < num_sites; i++) {

		site = sites[i];

		printf("Sorting '%s'...\n", site->name);

		qsort(site->files, site->num_files, sizeof(file_t *), sort_by_date);

	}


	// Build new list.
	for (i = 0; i < num_sites; i++) {

		site = sites[i];

		for (j = 0; j < site->num_files; j++) {

			// Is it newer than last time we checked? OR
			// Is it part of incpattern?
			if ((conf_incpattern &&
				 file_listmatch(conf_incpattern, site->files[j]->name)) ||
				(site->files[j]->date > site->last_check)) {

				if (site->use_lists && site_listsok(site, site->files[j]))
					continue;

				// If file already in the new list?
				if (file_find(new_files, num_new, site->files[j]->name))
					continue;

				file = file_dupe(site->files[j]);
				if (!file) continue;

				tmp = realloc(new_files,
							  sizeof(file_t *)*(num_new + 1));
				if (!tmp) {
					file_free(file);
					return;
				}

				new_files = tmp;

				new_files[ num_new ] = file;
				num_new++;

				continue;

			}

			// Older, since its sorted, stop. Unless
			// we are searching for something
			if (!conf_incpattern)
				break;

		} // all files

	} // all sites.

	printf("Sorting new list...%u\n", num_new);

	qsort(new_files, num_new, sizeof(file_t *), sort_by_date);



	printf("\n\n-----======> DISPLAY MATRIX <======-----\n\n");


	printf("T |");
	for(i = 0; i < num_sites; i++)
		printf("%-5.5s|", sites[i] ? sites[i]->name : "fail1");
	printf(" New Items");
	printf("\n");
	printf("--+");
	for(i = 0; i < num_sites; i++)
		printf("-----+");
	printf("-----------");
	printf("\n");

	for (j = 0; j < num_new; j++) {

		printf("%c |", new_files[j]->type ? 'd' : 'f');

		for(i = 0; i < num_sites; i++)
			printf("%-5.5s|",
				   status2str( site_status(sites[i], new_files[j]) ));

		printf(" %s", new_files[j]->name);

		printf("\n");
	}

	printf("\n\n");

}


void site_savecfg(void)
{
	int i;
	lion_t *save_file = NULL;

	// Assigning last_check
	for (i = 0; i < num_sites; i++) {

		if (sites[i]->num_files) {

			// Save last check for autoq
			sites[i]->last_check_autoq = sites[i]->last_check;

			sites[i]->last_check = sites[i]->files[0]->date;
			debugf("'%s' new last_check %lu\n", sites[i]->name,
				   sites[i]->last_check);
		}
	}

	// Saving info

	if (conf_do_save) {
        char *tmpname;

        tmpname = strdup(TMP_TEMPLATE);
        if (!mktemp(tmpname)) {
            debugf("failed to create tmpfile name.\n");
            return;
        }

		save_file = lion_open(tmpname,
							  O_WRONLY|O_TRUNC|O_CREAT,
							  0600, LION_FLAG_NONE, NULL);
		if (save_file) {
			lion_disable_read(save_file);
			debugf("saving conf...\n");
			conf_read(save_file);
			lion_close(save_file);

			rename(tmpname, conf_file_name);
		}

        SAFE_FREE(tmpname);

	}

}




char *status2str(status_t i)
{
	switch (i) {
	case STATUS_FAIL:
		return "fail";
	case STATUS_INC:
		return "INC";
	case STATUS_NUKED:
		return "NUKE";
	case STATUS_COMPLETE:
		return "    ";
	case STATUS_SKIP:
		return "skip";
	case STATUS_MISS:
	default:
		return "MISS";
	}
}

status_t site_status(site_t *site, file_t *file)
{
	static char buffer[1024];

	// Status of this file on this site
	if (!site || site->failed)
		return STATUS_FAIL;

	// Do we have the file?
	if (file_find(site->files, site->num_files, file->name)) {

        // is it nuked?
		if (site->nuketest) {
			snprintf(buffer, sizeof(buffer), site->nuketest,
					 file->name);
			if (file_find(site->files, site->num_files, buffer))
				return STATUS_NUKED;
        }

		// Is it incomplete?
		if (site->inctest) {
			snprintf(buffer, sizeof(buffer), site->inctest,
					 file->name);
			if (file_find(site->files, site->num_files, buffer))
				return STATUS_INC;
		}// intest

		// Its complete
		return STATUS_COMPLETE;
	}


	// Is it ignored by patterns?
	if (site_listsok(site, file))
		return STATUS_SKIP;


    // is it nuked?
    if (site->nuketest) {
        snprintf(buffer, sizeof(buffer), site->nuketest,
                 file->name);
        if (file_find(site->files, site->num_files, buffer))
            return STATUS_NUKED;
    }


	// It is missing.
	return STATUS_MISS;

}


int site_listsok(site_t *site, file_t *file)
{

	if (file->type == 1) {

		// Directory

		//
		if (site->dpasslist &&
			(!file_listmatch(site->dpasslist, file->name))) {
			return 2;
		}

		if (site->dskiplist &&
			(file_listmatch(site->dskiplist, file->name))) {
			return 3;
		}
		// Pass the directory.
		return 0;
	}

	if (site->fskipempty && !file->size)
		return 1;

	if (site->fpasslist &&
		(!file_listmatch(site->fpasslist, file->name))) {
		return 2;
	}

	if (site->fskiplist &&
		(file_listmatch(site->fskiplist, file->name))) {
		return 3;
	}
	// Pass the file.
	return 0;

}


site_t *site_find(char *name)
{
	int i;

	for (i = 0; i < num_sites; i++) {

		if (!sites[i]) continue;

		if (!mystrccmp(sites[i]->name, name))
			return sites[i];

	}

	return NULL;

}
