// $Id: command2.c,v 1.32 2010/10/12 02:36:32 lundman Exp $
//
// Jorgen Lundman 18th December 2003.
//
// Command Channel Functions. (GUI's talking to engine)
//
// Once a command connection is authenticated, we get handed a user from
// (command.c) to here. We then give full access here and handle all commands.
//
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lion.h"
#include "misc.h"

#include "debug.h"
#include "command2.h"
#include "settings.h"
#include "parser.h"
#include "engine.h"
#include "users.h"
#include "sites.h"
#include "session.h"
#include "manager.h"
#include "queue.h"


static unsigned int num_clients = 0;



static parser_command_list_t command_authenticated_list[] = {
	{ COMMAND( "QUIT"           ), command2_cmd_quit            },
	{ COMMAND( "HELP"           ), command2_cmd_help            },
	{ COMMAND( "WHO"            ), command2_cmd_who             },
	{ COMMAND( "SHUTDOWN"       ), command2_cmd_shutdown        },
	{ COMMAND( "SETPASS"        ), command2_cmd_setpass         },

	{ COMMAND( "SITEADD"        ), command2_cmd_siteadd         },
	{ COMMAND( "SITELIST"       ), command2_cmd_sitelist        },
	{ COMMAND( "SITEMOD"        ), command2_cmd_sitemod         },
	{ COMMAND( "SITEDEL"        ), command2_cmd_sitedel         },

	{ COMMAND( "SESSIONNEW"     ), command2_cmd_sessionnew      },
	{ COMMAND( "SESSIONFREE"    ), command2_cmd_sessionfree     },
	{ COMMAND( "DIRLIST"        ), command2_cmd_dirlist         },

	// Queue Commands
	{ COMMAND( "QUEUENEW"       ), command2_cmd_queuenew        },
	{ COMMAND( "QADD"           ), command2_cmd_qadd            },
	{ COMMAND( "QLIST"          ), command2_cmd_qlist           },
	{ COMMAND( "QGET"           ), command2_cmd_qget            },
	{ COMMAND( "QERR"           ), command2_cmd_qerr            },
	{ COMMAND( "QGRAB"          ), command2_cmd_qgrab           },
	{ COMMAND( "QDEL"           ), command2_cmd_qdel            },
	{ COMMAND( "QMOVE"          ), command2_cmd_qmove           },
	{ COMMAND( "QCOMPARE"       ), command2_cmd_qcompare        },
	{ COMMAND( "QUEUEFREE"      ), command2_cmd_queuefree       },
	{ COMMAND( "QCLONE"         ), command2_cmd_qclone          },


	{ COMMAND( "GO"             ), command2_cmd_go              },
	{ COMMAND( "STOP"           ), command2_cmd_stop            },


	// Actual FTPD RFC commands:
	{ COMMAND( "CWD"            ), command2_cmd_cwd             },
	{ COMMAND( "PWD"            ), command2_cmd_pwd             },
	{ COMMAND( "SIZE"           ), command2_cmd_size            },
	{ COMMAND( "DELE"           ), command2_cmd_dele            },
	{ COMMAND( "MKD"            ), command2_cmd_mkd             },
	{ COMMAND( "RMD"            ), command2_cmd_rmd             },
	{ COMMAND( "SITE"           ), command2_cmd_site            },
	{ COMMAND( "REN"            ), command2_cmd_ren             },
	{ COMMAND( "MDTM"           ), command2_cmd_mdtm            },

	{ COMMAND( "QUOTE"          ), command2_cmd_quote           },

	{ COMMAND( "SUBSCRIBE"      ), command2_cmd_subscribe       },
	{ COMMAND( "UNSUBSCRIBE"    ), command2_cmd_unsubscribe     },

	{ NULL,  0         ,                 NULL }
};




//
// We have a new authenticated user from command.c, let's set them up
//
void command2_register(lion_t *handle, char *name)
{
	command2_node_t *user;

	if (!handle)
		return;

	// Allocate a new node for this connection.
	user = (command2_node_t *) malloc(sizeof(*user));

	if (!user) {
		lion_close(handle);
		return;
	}

	memset(user, 0, sizeof(*user));

	user->name = strdup(name);
	user->handle = handle;

	lion_getpeername(handle, &user->host, &user->port);

	lion_set_userdata(handle, (void *) user);
	lion_set_handler(handle, command2_handler);

	debugf("[command2] registered '%s' %s:%u\n",
		   user->name,
		   lion_ntoa(user->host),
		   user->port);

	num_clients++;
}






// The main handler for the listening command channel, as well as
// "anonymous" connections. Once they have authenticated, we move them
// to a new handler.
//
int command2_handler(lion_t *handle, void *user_data,
					 int status, int size, char *line)
{
	command2_node_t *node = (command2_node_t *) user_data;

	switch(status) {

	case LION_CONNECTION_LOST:
	case LION_CONNECTION_CLOSED:

		if (node)
			debugf("[command2] connection %s (%s) %s:%u %d:%s\n",
				   size ? "lost" : "closed",
				   node->name ? node->name : "(null)",
				   lion_ntoa(node->host), node->port,
				   size, line ? line : "(null)");

		node->handle = NULL;

		// A user has disconnected. If Manager was the user's, we can
		// release it.
		// If the user was subscribed to a queue, unsubscribe
		manager_unlink_all_user(node);

	    queue_unsubscribe_all(node);

		// Free user?
		debugf("[command2] released user '%s' %p\n", node->name, node);
		SAFE_FREE(node->name);
		SAFE_FREE(node);

		num_clients--;

		break;



	case LION_INPUT:
		debugf("[command2] input '%s'\n", line);

		if (user_data)
			parser_command(command_authenticated_list, line, user_data);
		break;


	}

	return 0;

}












//
// PARSER COMMAND CALLBACKS
//




//
// Required:
// Optional:
//
void command2_cmd_quit(char **keys, char **values,
					   int items,void *optarg)
{
	command2_node_t *node = (command2_node_t *)optarg;

	//	if (node && node->handle)
	//lion_printf(node->handle, "GOODBYE\r\n");
	lion_close(node->handle);

}




//
// Required:
// Optional:
//
void command2_cmd_help(char **keys, char **values,
					   int items,void *optarg)
{
	command2_node_t *node = (command2_node_t *)optarg;
	int i;

	lion_printf(node->handle, "HELP|CODE=0|Msg=");

	for (i = 0;
		 command_authenticated_list[i].command;
		 i++) {

		lion_printf(node->handle, " %s",
					command_authenticated_list[i].command);

	}

	lion_printf(node->handle, ".\r\n");

}




static int command2_cmd_who_sub(lion_t *handle, void *arg1, void *arg2)
{
	command2_node_t *user;

	if (!arg1) return 0; // Stop.

	if (lion_get_handler(handle) != command2_handler)
		return 1; // iterate

	user = (command2_node_t *)lion_get_userdata(handle);

	if (!user) return 1; // iterate

	lion_printf((lion_t *)arg1,
				"WHO|NAME=%s|HOST=%s|PORT=%u|SSL=%s\r\n",
				user->name,
				lion_ntoa(user->host),
				user->port,
				user->handle && lion_ssl_enabled(user->handle) ? "yes" : "no");

	return 1; // iterate

}





//
// Required:
// Optional:
//
void command2_cmd_who(char **keys, char **values,
					  int items,void *optarg)
{
	command2_node_t *node = (command2_node_t *)optarg;

	lion_printf(node->handle, "WHO|BEGIN|CLIENTS=%u\r\n",
				num_clients);

	lion_find(command2_cmd_who_sub, (void *)node->handle, NULL);

	lion_printf(node->handle, "WHO|END|Msg=Command Completed.\r\n");

}




//
// Required:
// Optional: MSG
//
void command2_cmd_shutdown(char **keys, char **values,
						  int items,void *optarg)
{
	command2_node_t *node = (command2_node_t *)optarg;
	char *last;

	last = parser_findkey(keys, values, items, "LASTCLIENT");

	if (last && (num_clients != 1)) {
		lion_printf(node->handle,
					"SHUTDOWN|CODE=CODE=1403|MSG=Not last client. (clients = %d)\r\n",
					num_clients);
		return;
	}

	//commands2_tell_all("GOODBYE|")
	lion_printf(node->handle,
				"SHUTDOWN|CODE=0|Msg=Shutting down \r\n");

	engine_running = 0;

}

//
// Required: OLD, NEW
// Optional:
//
void command2_cmd_setpass(char **keys, char **values,
						  int items,void *optarg)
{
	command2_node_t *node = (command2_node_t *)optarg;
	char *old, *new;

	old = parser_findkey(keys, values, items, "OLD");
	new = parser_findkey(keys, values, items, "NEW");

	if (!old || !new) {
		lion_printf(node->handle,
					"SETPASS|CODE=1504|Msg=Required arguments: OLD, NEW (Optionals: )\r\n");
		return;
	}

	// Verify OLD password.
	if (!users_authenticate(node->name, old)) {
		lion_printf(node->handle, "SETPASS|CODE=1502|MSG=Login incorrect\r\n");
		return;
	}

	users_setpass(node->name, new);

	lion_printf(node->handle, "SETPASS|CODE=0|MSG=New password has been set.\r\n");

}







/* ***************************************************************** */












//
// Required: NAME, HOST, USER, PASS
// Optional: Too many
//
void command2_cmd_siteadd(char **keys, char **values,
						  int items,void *optarg)
{
	command2_node_t *node = (command2_node_t *)optarg;
	int ret;
	sites_t *newsite;

	// Fetch out required fields and assign left overs.
	newsite = sites_newnode();

	if (!newsite) {

		lion_printf(node->handle,
					"SITEADD|CODE=1401|MSG=Out of memory.\r\n");
		return;
	}


	// This checks for required fields, then saves optional.
	ret = sites_parsekeys(keys, values, items, newsite);

	if (ret) {
		if (ret == 1504)
			lion_printf(node->handle,
						"SITEADD|CODE=1504|Msg=Required arguments: NAME, HOST, USER, PASS (Optionals: many many)\r\n");
		else
			lion_printf(node->handle,
						"SITEADD|CODE=%u|Msg=failed to add new site\r\n",
						ret);

		sites_freenode(newsite);
		return;
	}

	// Chain it in
	sites_insertnode(newsite);

	// Save file to disk
	sites_save();

	lion_printf(node->handle,
				"SITEADD|CODE=0|SITEID=%u|Msg=Added successfully.\r\n",
				newsite->id);
}




//
// Required:
// Optional: SHORT, SITEID
//
void command2_cmd_sitelist(char **keys, char **values,
						  int items,void *optarg)
{
	command2_node_t *node = (command2_node_t *)optarg;
	char *shortlist, *thesite;
	int siteid = 0;

	shortlist = parser_findkey(keys, values, items, "SHORT");
	thesite   = parser_findkey(keys, values, items, "SITEID");

	// Optional sitelist.
	if (thesite) siteid = atoi(thesite);
	if (siteid < 0) siteid = 0;  // Illegal values.

	lion_printf(node->handle, "SITELIST|BEGIN\r\n");

	sites_listentries(node->handle,
					  siteid ? siteid :
					  ( shortlist ? SITES_LIST_ALL_SHORT :
						SITES_LIST_ALL));

	lion_printf(node->handle, "SITELIST|END\r\n");

}





//
// Required: SITEID
// Optional: Many many
//
void command2_cmd_sitemod(char **keys, char **values,
						  int items,void *optarg)
{
	command2_node_t *node = (command2_node_t *)optarg;
	int ret;
	sites_t *site;
	char *value;

	if (!(value = parser_findkey(keys, values, items, "SITEID"))) {
		lion_printf(node->handle,
					"SITEMOD|CODE=1501|Msg=Required arguments: SITEID\r\n");
		return;
	}

	// Find the site:
	if (!(site = sites_getid(atoi(value)))) {
		lion_printf(node->handle,
					"SITEMOD|CODE=1505|MSG=No such SITEID defined.\r\n");
		return;
	}


	// This checks for required fields, then saves optional.
	ret = sites_parsekeys(keys, values, items, site);

	// ret can be 504 for, required arguments missing, that's ok and is
	// ignored since user may only have modified other items.

	// Save file to disk
	sites_save();

	lion_printf(node->handle,
				"SITEMOD|CODE=0|SITEID=%u|Msg=Modified successfully.\r\n",
				site->id);
}






//
// Required: SITEID
// Optional:
//
void command2_cmd_sitedel(char **keys, char **values,
						  int items,void *optarg)
{
	command2_node_t *node = (command2_node_t *)optarg;
	sites_t *site;
	char *value;

	if (!(value = parser_findkey(keys, values, items, "SITEID"))) {
		lion_printf(node->handle,
					"SITEDEL|CODE=1501|Msg=Required arguments: SITEID\r\n");
		return;
	}

	// Find the site:
	if (!(site = sites_getid(atoi(value)))) {
		lion_printf(node->handle,
					"SITEDEL|CODE=1505|SITEID=%s|MSG=No such SITEID defined.\r\n",
					value);
		return;
	}

	sites_del(site);

	lion_printf(node->handle,
				"SITEDEL|CODE=0|SITEID=%s|MSG=Site deleted.\r\n",
				value);
}














void command2_cmd_sessionnew(char **keys, char **values,
						  int items,void *optarg)
{
	command2_node_t *node = (command2_node_t *)optarg;
	char *value;
	sites_t *site;
	int flags = 0;

	if (!(value = parser_findkey(keys, values, items, "SITEID"))) {
		lion_printf(node->handle,
					"SESSIONNEW|CODE=1501|Msg=Required arguments: SITEID\r\n");
		return;
	}

	// Find the site:
	if (!(site = sites_getid(atoi(value)))) {
		lion_printf(node->handle,
					"SESSIONNEW|CODE=1505|MSG=No such SITEID defined.\r\n");
		return;
	}


	// Optional argument
	value = parser_findkey(keys, values, items, "LOG");
	if (value && (atoi(value) != 0))
		flags |= MANAGER_USERFLAG_LOG;

	manager_new(node, site, flags);

}


void command2_cmd_sessionfree(char **keys, char **values,
						  int items,void *optarg)
{
	command2_node_t *node = (command2_node_t *)optarg;
	manager_t *mgr;
	char *value;

	if (!(value = parser_findkey(keys, values, items, "SID"))) {
		lion_printf(node->handle,
					"SESSIONFREE|CODE=1501|Msg=Required arguments: SID\r\n");
		return;
	}


	// Find the session:
	if (!(mgr = manager_find_fromsid(atoi(value)))) {
		lion_printf(node->handle,
					"SESSIONFREE|CODE=1506|MSG=No such SID defined.\r\n");
		return;
	}

	if (mgr->qid) {
		lion_printf(node->handle,
					"SESSIONFREE|CODE=1518|MSG=SID %s is in queue QID %u, please release SID first.\r\n",
					value,
					mgr->qid);
		return;
	}





	manager_release(mgr);

	lion_printf(node->handle, "SESSIONFREE|CODE=0|SID=%d|MSG=Success\r\n",
				atoi(value));

}



//
// Required: SID
// Optional: PATH, ARGS, RAW, CACHEOK, LOG
//
void command2_cmd_dirlist(char **keys, char **values,
						  int items,void *optarg)
{
	command2_node_t *node = (command2_node_t *)optarg;
	manager_t *mgr;
	char *sid, *path, *args, *raw, *log, *cacheok;
	int flags = 0;

	if (!(sid = parser_findkey(keys, values, items, "SID"))) {
		lion_printf(node->handle,
					"DIRLIST|CODE=1501|Msg=Required arguments: SID (Optional: PATH,ARGS,RAW,CACHEOK,LOG)\r\n");
		return;
	}

	debugf("dirlist sid %s\n", sid);

	path    = parser_findkey(keys, values, items, "PATH");
	args    = parser_findkey(keys, values, items, "ARGS");
	raw     = parser_findkey(keys, values, items, "RAW");
    cacheok = parser_findkey(keys, values, items, "CACHEOK");
    log     = parser_findkey(keys, values, items, "LOG");

	// Find the session:
	if (!(mgr = manager_find_fromsid(atoi(sid)))) {
		lion_printf(node->handle,
					"DIRLIST|SID=%s|CODE=1506|MSG=No such SID defined.\r\n",
					sid);
		return;
	}

	// Do we own this?
	if (mgr->user != node) {
		lion_printf(node->handle,
					"DIRLIST|SID=%u|CODE=1508|MSG=You do not control the SID.\r\n",
					mgr->id);
		return;
	}


	if (raw)
		flags |= SESSION_CMDQ_FLAG_RAWLIST;

	if (cacheok)
		flags |= SESSION_CMDQ_FLAG_CACHEOK;

	if (log)
		flags |= SESSION_CMDQ_FLAG_LOG;

	manager_dirlist(mgr, args, path, flags);

}


//
// Required: SID, MSG
// Optional: LOG
//
void command2_cmd_quote(char **keys, char **values,
						int items,void *optarg)
{
	command2_node_t *node = (command2_node_t *)optarg;
	manager_t *mgr;
	char *sid, *msg, *log;
	int flags = 0;

	// Required
	sid = parser_findkey(keys, values, items, "SID");
	msg = parser_findkey(keys, values, items, "MSG");

	// Optional
	log = parser_findkey(keys, values, items, "LOG");

	if (!sid || !msg) {
		lion_printf(node->handle,
					"QUOTE|CODE=1501|Msg=Required arguments: SID, MSG.  (Optional: LOG.)\r\n");
		return;
	}


	// Find the session:
	if (!(mgr = manager_find_fromsid(atoi(sid)))) {
		lion_printf(node->handle,
					"QUOTE|SID=%s|CODE=1506|MSG=No such SID defined.\r\n",
					sid);
		return;
	}

	// Do we own this?
	if (mgr->user != node) {
		lion_printf(node->handle,
					"QUOTE|SID=%u|CODE=1508|MSG=You do not control the SID.\r\n",
					mgr->id);
		return;
	}

	if (log)
		flags |= SESSION_CMDQ_FLAG_LOG;

	manager_quote(mgr, msg, flags);

}


//
// Required: SID, PATH or FID
// Optional: LOG
//
void command2_cmd_cwd(char **keys, char **values,
					  int items,void *optarg)
{
	command2_node_t *node = (command2_node_t *)optarg;
	manager_t *mgr;
	char *sid, *path, *fid, *log;
	int flags = 0;


	// Required
	sid = parser_findkey(keys, values, items, "SID");
	path= parser_findkey(keys, values, items, "PATH");
	fid = parser_findkey(keys, values, items, "FID");

	// Optional
	log = parser_findkey(keys, values, items, "LOG");


	if (!sid) {
		lion_printf(node->handle,
					"CWD|CODE=1501|Msg=Required arguments: SID, PATH or FID.  (Optional: LOG.)\r\n");
		return;
	}


	// Find the session:
	if (!(mgr = manager_find_fromsid(atoi(sid)))) {
		lion_printf(node->handle,
					"CWD|SID=%s|CODE=1506|MSG=No such SID defined.\r\n",
					sid);
		return;
	}

	// Do we own this?
	if (mgr->user != node) {
		lion_printf(node->handle,
					"CWD|SID=%u|CODE=1508|MSG=You do not control the SID.\r\n",
					mgr->id);
		return;
	}


	if (!path && fid && *fid) {
		file_t *file;

		file = session_getfid(mgr->session, atoi(fid));

		if (!file) {
			lion_printf(node->handle,
						"CWD|SID=%s|CODE=1515|MSG=No such FID in current cache: %s \r\n",
						sid, fid);
			return;
		}

		if (file) path = file->fullpath;

	}


	if (!path) {
		lion_printf(node->handle,
					"CWD|SID=%s|CODE=1501|Msg=Required arguments: SID, PATH or FID.  (Optional: LOG.)\r\n", sid);
		return;
	}


	// Do we define a new command for each of these, or use a generic?
	if (log)
		flags |= SESSION_CMDQ_FLAG_LOG;

	manager_cwd(mgr, path, flags);

}


//
// Required: SID
// Optional: LOG
//
void command2_cmd_pwd(char **keys, char **values,
					  int items,void *optarg)
{
	command2_node_t *node = (command2_node_t *)optarg;
	manager_t *mgr;
	char *sid, *log;
	int flags = 0;

	// Required
	sid = parser_findkey(keys, values, items, "SID");

	// Optional
	log = parser_findkey(keys, values, items, "LOG");


	if (!sid) {
		lion_printf(node->handle,
					"PWD|CODE=1501|Msg=Required arguments: SID.  (Optional: LOG.)\r\n");
		return;
	}


	// Find the session:
	if (!(mgr = manager_find_fromsid(atoi(sid)))) {
		lion_printf(node->handle,
					"PWD|SID=%s|CODE=1506|MSG=No such SID defined.\r\n",
					sid);
		return;
	}

	// Do we own this?
	if (mgr->user != node) {
		lion_printf(node->handle,
					"PWD|SID=%u|CODE=1508|MSG=You do not control the SID.\r\n",
					mgr->id);
		return;
	}

	// Do we define a new command for each of these, or use a generic?
	if (log)
		flags |= SESSION_CMDQ_FLAG_LOG;

	manager_pwd(mgr, flags);

}





//
// Required: SID, PATH or FID
// Optional: LOG
//
void command2_cmd_size(char **keys, char **values,
					   int items,void *optarg)
{
	command2_node_t *node = (command2_node_t *)optarg;
	manager_t *mgr;
	char *sid, *path, *fid, *log;
	int flags = 0;

	// Required
	sid = parser_findkey(keys, values, items, "SID");
	path= parser_findkey(keys, values, items, "PATH");
	fid = parser_findkey(keys, values, items, "FID");

	// Optional
	log = parser_findkey(keys, values, items, "LOG");


	if (!sid) {
		lion_printf(node->handle,
					"SIZE|CODE=1501|Msg=Required arguments: SID, PATH or FID.  (Optional: LOG.)\r\n");
		return;
	}


	// Find the session:
	if (!(mgr = manager_find_fromsid(atoi(sid)))) {
		lion_printf(node->handle,
					"SIZE|SID=%s|CODE=1506|MSG=No such SID defined.\r\n",
					sid);
		return;
	}

	// Do we own this?
	if (mgr->user != node) {
		lion_printf(node->handle,
					"SIZE|SID=%u|CODE=1508|MSG=You do not control the SID.\r\n",
					mgr->id);
		return;
	}


	if (path)
		path = misc_url_decode(path);


	if (!path && fid && *fid) {
		file_t *file;

		file = session_getfid(mgr->session, atoi(fid));

		if (!file) {
			lion_printf(node->handle,
						"SIZE|SID=%s|CODE=1515|MSG=No such FID in current cache: %s \r\n",
						sid, fid);
			return;
		}

		if (file) path = strdup(file->fullpath);

	}


	if (!path) {
		lion_printf(node->handle,
					"SIZE|SID=%s|CODE=1501|Msg=Required arguments: SID, PATH or FID.  (Optional: LOG.)\r\n", sid);
		return;
	}



	// Do we define a new command for each of these, or use a generic?
	if (log)
		flags |= SESSION_CMDQ_FLAG_LOG;

	manager_size(mgr, path, flags);

	SAFE_FREE(path);

}



//
// Required: SID1, SID2
// Optional:
//
void command2_cmd_queuenew(char **keys, char **values,
						   int items,void *optarg)
{
	command2_node_t *node = (command2_node_t *)optarg;
	manager_t *mgr1, *mgr2;
	char *sid1, *sid2;

	// Required, north_sea and south_sea!
	sid1 = parser_findkey(keys, values, items, "NORTH_SID");
	sid2 = parser_findkey(keys, values, items, "SOUTH_SID");


	if (!sid1||!sid2) {
		lion_printf(node->handle,
					"QUEUENEW|CODE=1501|Msg=Required arguments: NORTH_SID, SOUTH_SID (Optional: )\r\n");
		return;
	}


	// Find the session:
	if (!(mgr1 = manager_find_fromsid(atoi(sid1)))) {
		lion_printf(node->handle,
					"QUEUENEW|SID=%s|CODE=1506|MSG=No such SID defined.\r\n",
					sid1);
		return;
	}

	if (!(mgr2 = manager_find_fromsid(atoi(sid2)))) {
		lion_printf(node->handle,
					"QUEUENEW|SID=%s|CODE=1506|MSG=No such SID defined.\r\n",
					sid2);
		return;
	}

	if (mgr1 == mgr2) {
		lion_printf(node->handle,
					"QUEUENEW|CODE=1511|MSG=Invalid argument: both SIDs are the same.\r\n");
		return;
	}

	// Do we own this?
	if (mgr1->user != node) {
		lion_printf(node->handle,
					"QUEUENEW|SID=%u|CODE=1508|MSG=You do not control the NORTH SID.\r\n",
					mgr1->id);
		return;
	}

	if (mgr2->user != node) {
		lion_printf(node->handle,
					"QUEUENEW|SID=%u|CODE=1508|MSG=You do not control the SOUTH SID.\r\n",
					mgr2->id);
		return;
	}


	manager_queuenew(mgr1, mgr2);

}




//
// Required: QID, SRC=NORTH|SOUTH (One of FID/SRCNAME/SRCPATH)
// Optional: FID, SRCNAME, SRCPATH, SRCSIZE, SRCREST,
//                DSTNAME, DSTPATH, DSTSIZE, DSTREST,
//                RESUME | OVERWRITE, QTYPE
//                @,
//
void command2_cmd_qadd(char **keys, char **values,
					   int items,void *optarg)
{
	command2_node_t *node = (command2_node_t *)optarg;
	manager_t *mgr_src, *mgr_dst;
	char *qid, *src, *fid, *qtype;
	queue_t *queue;
	qitem_t *qitem;
	char *srcname, *srcpath, *srcsize, *srcrest;
	char *dstname, *dstpath, *dstsize, *dstrest;
	char *resume, *overwrite, *at;

	// We have to have one of FID/SRCNAME/SRCPATH
	qid     = parser_findkey(keys, values, items, "QID");
	src     = parser_findkey(keys, values, items, "SRC");

	if (!qid || !src) {
		lion_printf(node->handle,
					"QADD|CODE=1501|Msg=Required arguments: QID, SRC [one of FID, SRCNAME, SRCPATH, QTYPE=STOP] (Optionals: SRCNAME,SRCPATH,SRCSIZE,SRCREST,DSTNAME,DSTPATH,DSTREST,RESUME,OVERWRITE,@,TYPE )\r\n");
		return;
	}


	queue = queue_findbyqid(atoi(qid));

	if (!queue) {
		lion_printf(node->handle,
					"QADD|CODE=1512|MSG=No such QID defined: %d\r\n",
					atoi(qid));
		return;
	}


	fid     = parser_findkey(keys, values, items, "FID");
	srcname = parser_findkey(keys, values, items, "SRCNAME");
	srcpath = parser_findkey(keys, values, items, "SRCPATH");
	qtype   = parser_findkey(keys, values, items, "QTYPE");

	if (!((fid!=NULL) | (srcname!=NULL) | (srcpath!=NULL) | (qtype!=NULL))) {
		lion_printf(node->handle,
					"QADD|CODE=1501|Msg=Must specify at least one of: FID, SRCNAME, SRCPATH or QTYPE=STOP.\r\n");
		return;
	}




	srcsize = parser_findkey(keys, values, items, "SRCSIZE");
	srcrest = parser_findkey(keys, values, items, "SRCREST");

	dstname = parser_findkey(keys, values, items, "DSTNAME");
	dstpath = parser_findkey(keys, values, items, "DSTPATH");
	dstsize = parser_findkey(keys, values, items, "DSTSIZE");
	dstrest = parser_findkey(keys, values, items, "DSTREST");

	resume  = parser_findkey(keys, values, items, "RESUME");
	overwrite=parser_findkey(keys, values, items, "OVERWRITE");

	// Positional modifier
	at      = parser_findkey(keys, values, items, "@");



	// Grab a new queue item node - finally.
	qitem = queue_newitem(queue);


	if (!mystrccmp("NORTH", src))
		qitem->src_is_north = 1;
	else if (!mystrccmp("SOUTH", src))
		qitem->src_is_north = 0;
	else {

		queue_freeitem(qitem);
		lion_printf(node->handle,
					"QADD|QID=%s|CODE=1513|MSG=SOURCE must be one of NORTH or SOUTH.\r\n",
					qid);
		return;
	}


	// Assign over the managers. Error checking?
	//queue->north_mgr = manager_find_fromsid(queue->north_sid);
	//queue->south_mgr = manager_find_fromsid(queue->south_sid);


	mgr_src =  qitem->src_is_north ? queue->north_mgr : queue->south_mgr;
	mgr_dst = !qitem->src_is_north ? queue->north_mgr : queue->south_mgr;


	// Go fetch the FID, this is so we can assign everything we know of
	// the FID, and still be able to re-assign it manually.
	if (fid && *fid) {
		file_t *file = NULL;

		if (mgr_src && mgr_src->session) {

			file = session_getfid(mgr_src->session, atoi(fid));

		}

		// If they ASKED for a FID but we can't find it, we should fail
		// here.
		if (!file) {
			lion_printf(node->handle,
						"QADD|QID=%s|CODE=1515|MSG=No such FID in current cache: %s \r\n",
						qid, fid);
			return;
		}


		// Make a copy of this item.
		file_dupe(&qitem->src, file);

		//memcpy(&qitem->src, file, sizeof(qitem->src));
		//memcpy(&qitem->dst, file, sizeof(qitem->dst));

		if (file->directory == YNA_YES)
			qitem->type = QITEM_TYPE_DIRECTORY;
		else
			qitem->type = QITEM_TYPE_FILE;

		// Since FID was given, we manually assign it over, so that
		// we can reply with FID. It is then cleared.
		qitem->src.fid = file->fid;

	}

	// Assign values over to qitem.
	if (qtype && *qtype) {
		qitem->type = queue_str2type(qtype);
	}


	// STOP item?
	if (qitem->type == QITEM_TYPE_STOP) {
		queue_insert(queue, qitem, at, node->handle);
		return;
	}



	// SOURCE information

	if (srcname && *srcname) {
		SAFE_FREE(qitem->src.name);
		qitem->src.name = misc_url_decode(srcname);
		SAFE_FREE(qitem->src.fullpath);
		// Build full path. Done in makepath
	}
	if (srcpath && *srcpath) {
		SAFE_FREE(qitem->src.fullpath);
		qitem->src.fullpath = misc_url_decode(srcpath);
	}

	if (srcsize && *srcsize)
		qitem->src.size = strtoull(srcsize, NULL, 10);

	if (srcrest && *srcrest)
		qitem->src.rest = strtoull(srcrest, NULL, 10);


	// Make sure fullpath, name and dirname are all set.
	file_makepath(&qitem->src,
				  mgr_src && mgr_src->session ?
				  mgr_src->session->pwd : NULL);



	// DESTINATION information

	if (dstname && *dstname) {
		SAFE_FREE(qitem->dst.name);
		qitem->dst.name = misc_url_decode(dstname);
		SAFE_FREE(qitem->dst.fullpath);
		// Build full path. Done in makedest()
	}
	if (dstpath && *dstpath) {
		SAFE_FREE(qitem->dst.fullpath);
		qitem->dst.fullpath = misc_url_decode(dstpath);
	}


	if (dstsize && *dstsize)
		qitem->dst.size = strtoull(dstsize, NULL, 10);

	if (dstrest && *dstrest)
		qitem->dst.rest = strtoull(dstrest, NULL, 10);


	file_makedest(&qitem->dst, &qitem->src,
				  mgr_dst && mgr_dst->session ?
				  mgr_dst->session->pwd : NULL);

	if (resume)
		qitem->flags |= QITEM_FLAG_RESUME;
	else if (overwrite)
		qitem->flags |= QITEM_FLAG_OVERWRITE;


	// Set the default flags for this. Fetch out resume/overwrite
	// settings for the site, then overwrite settings with whatever passed
	// along here.

	// Stop them from inserting at pos = 0 if we are active.
	if ((queue->state != QUEUE_IDLE) &&
		at &&
		atoi(at) == 0)
		at = "1";

	// This sends the OK back to node.
	queue_insert(queue, qitem, at ? at : "LAST", node->handle);

}



//
// Required: QID
// Optional:
//
void command2_cmd_qlist(char **keys, char **values,
						int items,void *optarg)
{
	command2_node_t *node = (command2_node_t *)optarg;

	queue_list(node);

}



//
// Required: QID
// Optional:
//
void command2_cmd_qget(char **keys, char **values,
						int items,void *optarg)
{
	command2_node_t *node = (command2_node_t *)optarg;
	char *qid;
	queue_t *queue;


	qid     = parser_findkey(keys, values, items, "QID");

	if (!qid) {
		lion_printf(node->handle,
					"QGET|CODE=1501|Msg=Required arguments: QID. (Optionals: \r\n");
		return;
	}


	queue = queue_findbyqid(atoi(qid));

	if (!queue) {
		lion_printf(node->handle,
					"QGET|CODE=1512|MSG=No such QID defined: %d\r\n",
					atoi(qid));
		return;
	}

	queue_get(queue, node->handle);

}




//
// Required: QID
// Optional:
//
void command2_cmd_qerr(char **keys, char **values,
						int items,void *optarg)
{
	command2_node_t *node = (command2_node_t *)optarg;
	char *qid;
	queue_t *queue;


	qid     = parser_findkey(keys, values, items, "QID");

	if (!qid) {
		lion_printf(node->handle,
					"QERR|CODE=1501|Msg=Required arguments: QID. (Optionals: \r\n");
		return;
	}


	queue = queue_findbyqid(atoi(qid));

	if (!queue) {
		lion_printf(node->handle,
					"QERR|CODE=1512|MSG=No such QID defined: %d\r\n",
					atoi(qid));
		return;
	}

	queue_err(queue, node->handle);

}





//
// Required: QID
// Optional: SUBSCRIBE
//
void command2_cmd_qgrab(char **keys, char **values,
						int items,void *optarg)
{
	command2_node_t *node = (command2_node_t *)optarg;
	char *qid, *subscribe;
	queue_t *queue;
	int flags = 0;

	qid     = parser_findkey(keys, values, items, "QID");

	if (!qid) {
		lion_printf(node->handle,
					"QGRAB|CODE=1501|Msg=Required arguments: QID. (Optionals: \r\n");
		return;
	}


	queue = queue_findbyqid(atoi(qid));

	if (!queue) {
		lion_printf(node->handle,
					"QGRAB|CODE=1512|MSG=No such QID defined: %d\r\n",
					atoi(qid));
		return;
	}

	subscribe = parser_findkey(keys, values, items, "SUBSCRIBE");

	if (subscribe)
		flags |= 1;

	queue_grab(queue, node, flags);

}







//
// Required: QID, @
// Optional:
//
void command2_cmd_qdel(char **keys, char **values,
						int items,void *optarg)
{
	command2_node_t *node = (command2_node_t *)optarg;
	char *qid, *at, *pos;
	queue_t *queue;


	qid     = parser_findkey(keys, values, items, "QID");
	at      = parser_findkey(keys, values, items, "@");

	if (!qid||!at) {
		lion_printf(node->handle,
					"QDEL|CODE=1501|Msg=Required arguments: QID, @. (Optionals: \r\n");
		return;
	}


	queue = queue_findbyqid(atoi(qid));

	if (!queue) {
		lion_printf(node->handle,
					"QDEL|CODE=1512|MSG=No such QID defined: %d\r\n",
					atoi(qid));
		return;
	}

	while((pos = misc_digtoken(&at, ",\r\n"))) {
		queue_del(queue, pos, node->handle);
	}
}




//
// Required: QID, FROM, TO
// Optional:
//
void command2_cmd_qmove(char **keys, char **values,
						int items,void *optarg)
{
	command2_node_t *node = (command2_node_t *)optarg;
	char *qid, *from, *to;
	queue_t *queue;


	qid     = parser_findkey(keys, values, items, "QID");
	from    = parser_findkey(keys, values, items, "FROM");
	to      = parser_findkey(keys, values, items, "TO");

	if (!qid || !from || !to) {
		lion_printf(node->handle,
					"QMOVE|CODE=1501|Msg=Required arguments: QID, FROM, TO. (Optionals: \r\n");
		return;
	}


	queue = queue_findbyqid(atoi(qid));

	if (!queue) {
		lion_printf(node->handle,
					"QMOVE|CODE=1512|MSG=No such QID defined: %d\r\n",
					atoi(qid));
		return;
	}

	queue_move(queue, from, to, node->handle);

}





//
// Required: QID
// Optional:
//
void command2_cmd_qcompare(char **keys, char **values,
						int items,void *optarg)
{
	command2_node_t *node = (command2_node_t *)optarg;
	char *qid;
	queue_t *queue;


	qid     = parser_findkey(keys, values, items, "QID");


	if (!qid) {
		lion_printf(node->handle,
					"QCOMPARE|CODE=1501|Msg=Required arguments: QID. (Optionals: \r\n");
		return;
	}


	queue = queue_findbyqid(atoi(qid));

	if (!queue) {
		lion_printf(node->handle,
					"QCOMPARE|CODE=1512|MSG=No such QID defined: %d\r\n",
					atoi(qid));
		return;
	}

	queue_compare(queue, node->handle);

}












//
// Required: QID
// Optional: IFIDLE
//
void command2_cmd_queuefree(char **keys, char **values,
							int items,void *optarg)
{
	command2_node_t *node = (command2_node_t *)optarg;
	queue_t *queue;
	char *qid, *ifidle;

	qid     = parser_findkey(keys, values, items, "QID");

	if (!qid) {
		lion_printf(node->handle,
					"QUEUEFREE|CODE=1501|Msg=Required arguments: QID. (Optionals: IFIDLE\r\n");
		return;
	}

	ifidle   = parser_findkey(keys, values, items, "IFIDLE");

	queue = queue_findbyqid(atoi(qid));

	if (!queue) {
		lion_printf(node->handle,
					"QUEUEFREE|CODE=1512|MSG=No such QID defined: %d\r\n",
					atoi(qid));
		return;
	}

	if (ifidle &&
		!queue_isidle(queue)) {

		lion_printf(node->handle,
					"QUEUEFREE|CODE=1402|QID=%s|MSG=Queue is not idle.\r\n",
					qid);
		return;
	}


	queue_unlink(queue);
	queue_freenode(queue);

	lion_printf(node->handle,
				"QUEUEFREE|CODE=0|QID=%s|Msg=Queue released.\r\n",
				qid);
}




//
// Required: QID
// Optional: START
//
void command2_cmd_qclone(char **keys, char **values,
                         int items,void *optarg)
{
	command2_node_t *node = (command2_node_t *)optarg;
	queue_t *queue;
	char *qid, *start, *sub;
    yesnoauto_t xstart;
    int active = 0;

	qid     = parser_findkey(keys, values, items, "QID");

	if (!qid) {
		lion_printf(node->handle,
					"QUEUEFREE|CODE=1501|Msg=Required arguments: QID. (Optionals: START, SUBSCRIBE\r\n");
		return;
	}

	start   = parser_findkey(keys, values, items, "START");

	queue = queue_findbyqid(atoi(qid));

	if (!queue) {
		lion_printf(node->handle,
					"QUEUEFREE|CODE=1512|MSG=No such QID defined: %d\r\n",
					atoi(qid));
		return;
	}

    // Remember if the queue is active before we lose it.
    active = (queue->state == QUEUE_IDLE) ? 0 : 1;

    // get new queue, if we can.
    queue = queue_clone(queue, node);

    if (!queue) {
        lion_printf(node->handle,
                    "QCLONE|CODE=1234|QID=%s|Msg=Failed to clone queue.\r\n",
                    qid);
        return;
    }

	sub     = parser_findkey(keys, values, items, "SUBSCRIBE");
	if (sub) {
		queue_subscribe(queue, node);
	}

    // Clone done.
	lion_printf(node->handle,
				"QCLONE|CODE=0|QID=%s|NEW_QID=%u|Msg=Cloned and created queue\r\n",
				qid,
                queue->id);

    // Start queue if wanted here: That way the GO message will be
    // received after the QCLONE creation response.
    xstart = str2yna(start);

    debugf("[command2] qclone: should start queue %u\n", xstart);

    if ((xstart == YNA_YES) ||
        ((xstart == YNA_AUTO) &&
         (active))
        ) {

        queue_go(queue, node->handle);

    }

}


















//
// Required: QID
// Optional: SUBSCRIBE
//
void command2_cmd_go(char **keys, char **values,
					 int items,void *optarg)
{
	command2_node_t *node = (command2_node_t *)optarg;
	char *qid, *sub;
	queue_t *queue;


	qid     = parser_findkey(keys, values, items, "QID");

	if (!qid) {
		lion_printf(node->handle,
					"GO|CODE=1501|Msg=Required arguments: QID. (Optionals: SUBSCRIBE\r\n");
		return;
	}


	queue = queue_findbyqid(atoi(qid));

	if (!queue) {
		lion_printf(node->handle,
					"GO|CODE=1512|MSG=No such QID defined: %d\r\n",
					atoi(qid));
		return;
	}


	sub     = parser_findkey(keys, values, items, "SUBSCRIBE");
	if (sub) {
		queue_subscribe(queue, node);
	}


	// Start processing the queue.
	// This means the user relinquishes control of the SIDs and
	// hands it over to the Queue processor.

	queue_go(queue, node->handle);

}



//
// Required: QID
// Optional: HARD, DROP
//
void command2_cmd_stop(char **keys, char **values,
					   int items,void *optarg)
{
	command2_node_t *node = (command2_node_t *)optarg;
	char *qid, *hard, *drop;
	queue_t *queue;
	int level;

	qid     = parser_findkey(keys, values, items, "QID");

	if (!qid) {
		lion_printf(node->handle,
					"STOP|CODE=1501|Msg=Required arguments: QID. (Optionals: HARD, STOP)\r\n");
		return;
	}


	queue = queue_findbyqid(atoi(qid));

	if (!queue) {
		lion_printf(node->handle,
					"STOP|CODE=1512|MSG=No such QID defined: %d\r\n",
					atoi(qid));
		return;
	}


	level = 0;
	hard  = parser_findkey(keys, values, items, "HARD");
	drop  = parser_findkey(keys, values, items, "DROP");

	if (hard) level = 1;
	if (drop) level = 2;


	// Start processing the queue.
	// This means the user relinquishes control of the SIDs and
	// hands it over to the Queue processor.


	queue_stop(queue, node->handle, level);

}








//
// Required: SID, PATH or FID
// Optional: LOG
//
void command2_cmd_dele(char **keys, char **values,
					   int items,void *optarg)
{
	command2_node_t *node = (command2_node_t *)optarg;
	manager_t *mgr;
	char *sid, *path, *log, *fid;
	int flags = 0;

	// Required
	sid = parser_findkey(keys, values, items, "SID");
	path= parser_findkey(keys, values, items, "PATH");
	fid = parser_findkey(keys, values, items, "FID");


	// Optional
	log = parser_findkey(keys, values, items, "LOG");

	if (!sid) {
		lion_printf(node->handle,
					"DELE|CODE=1501|Msg=Required arguments: SID, PATH or FID.  (Optional: LOG.)\r\n");
		return;
	}


	// Find the session:
	if (!(mgr = manager_find_fromsid(atoi(sid)))) {
		lion_printf(node->handle,
					"DELE|SID=%s|CODE=1506|MSG=No such SID defined.\r\n",
					sid);
		return;
	}

	// Do we own this?
	if (mgr->user != node) {
		lion_printf(node->handle,
					"DELE|SID=%u|CODE=1508|MSG=You do not control the SID.\r\n",
					mgr->id);
		return;
	}


	if (!path && fid && *fid) {
		file_t *file;

		file = session_getfid(mgr->session, atoi(fid));

		if (!file) {
			lion_printf(node->handle,
						"DELE|SID=%s|CODE=1515|MSG=No such FID in current cache: %s \r\n",
						sid, fid);
			return;
		}

		if (file) path = file->fullpath;

	}


	if (!path) {
		lion_printf(node->handle,
					"DELE|SID=%s|CODE=1501|Msg=Required arguments: SID, PATH or FID.  (Optional: LOG.)\r\n", sid);
		return;
	}



	// Do we define a new command for each of these, or use a generic?
	if (log)
		flags |= SESSION_CMDQ_FLAG_LOG;

	manager_dele(mgr, path, flags);

}









//
// Required: SID, PATH
// Optional: LOG
//
void command2_cmd_mkd(char **keys, char **values,
					  int items,void *optarg)
{
	command2_node_t *node = (command2_node_t *)optarg;
	manager_t *mgr;
	char *sid, *path, *log;
	int flags = 0;

	// Required
	sid = parser_findkey(keys, values, items, "SID");
	path= parser_findkey(keys, values, items, "PATH");

	// Optional
	log = parser_findkey(keys, values, items, "LOG");


	if (!sid||!path) {
		lion_printf(node->handle,
					"MKD|CODE=1501|Msg=Required arguments: SID, PATH.  (Optional: LOG.)\r\n");
		return;
	}


	// Find the session:
	if (!(mgr = manager_find_fromsid(atoi(sid)))) {
		lion_printf(node->handle,
					"MKD|SID=%s|CODE=1506|MSG=No such SID defined.\r\n",
					sid);
		return;
	}

	// Do we own this?
	if (mgr->user != node) {
		lion_printf(node->handle,
					"MKD|SID=%u|CODE=1508|MSG=You do not control the SID.\r\n",
					mgr->id);
		return;
	}

	// Do we define a new command for each of these, or use a generic?
	if (log)
		flags |= SESSION_CMDQ_FLAG_LOG;

	manager_mkd(mgr, path, flags);

}






//
// Required: SID, PATH or FID
// Optional: LOG
//
void command2_cmd_rmd(char **keys, char **values,
					  int items,void *optarg)
{
	command2_node_t *node = (command2_node_t *)optarg;
	manager_t *mgr;
	char *sid, *path, *fid, *log;
	int flags = 0;

	// Required
	sid = parser_findkey(keys, values, items, "SID");
	path= parser_findkey(keys, values, items, "PATH");
	fid = parser_findkey(keys, values, items, "FID");

	// Optional
	log = parser_findkey(keys, values, items, "LOG");


	if (!sid) {
		lion_printf(node->handle,
					"RMD|CODE=1501|Msg=Required arguments: SID, PATH or FID.  (Optional: LOG.)\r\n");
		return;
	}


	// Find the session:
	if (!(mgr = manager_find_fromsid(atoi(sid)))) {
		lion_printf(node->handle,
					"RMD|SID=%s|CODE=1506|MSG=No such SID defined.\r\n",
					sid);
		return;
	}

	// Do we own this?
	if (mgr->user != node) {
		lion_printf(node->handle,
					"RMD|SID=%u|CODE=1508|MSG=You do not control the SID.\r\n",
					mgr->id);
		return;
	}


	if (path)
		path = misc_url_decode(path);

	if (!path && fid && *fid) {
		file_t *file;

		file = session_getfid(mgr->session, atoi(fid));

		if (!file) {
			lion_printf(node->handle,
						"RMD|SID=%s|CODE=1515|MSG=No such FID in current cache: %s \r\n",
						sid, fid);
			return;
		}

		if (file) path = strdup(file->fullpath);

	}


	if (!path) {
		lion_printf(node->handle,
					"RMD|SID=%s|CODE=1501|Msg=Required arguments: SID, PATH or FID.  (Optional: LOG.)\r\n", sid);
		return;
	}



	// Do we define a new command for each of these, or use a generic?
	if (log)
		flags |= SESSION_CMDQ_FLAG_LOG;

	manager_rmd(mgr, path, flags);

	SAFE_FREE(path);

}






//
// Required: SID, CMD
// Optional: LOG
//
void command2_cmd_site(char **keys, char **values,
					   int items,void *optarg)
{
	command2_node_t *node = (command2_node_t *)optarg;
	manager_t *mgr;
	char *sid, *path, *log;
	int flags = 0;

	// Required
	sid = parser_findkey(keys, values, items, "SID");
	path= parser_findkey(keys, values, items, "CMD");

	// Optional
	log = parser_findkey(keys, values, items, "LOG");


	if (!sid||!path) {
		lion_printf(node->handle,
					"SITE|CODE=1501|Msg=Required arguments: SID, CMD.  (Optional: LOG.)\r\n");
		return;
	}


	// Find the session:
	if (!(mgr = manager_find_fromsid(atoi(sid)))) {
		lion_printf(node->handle,
					"SITE|SID=%s|CODE=1506|MSG=No such SID defined.\r\n",
					sid);
		return;
	}

	// Do we own this?
	if (mgr->user != node) {
		lion_printf(node->handle,
					"SITE|SID=%u|CODE=1508|MSG=You do not control the SID.\r\n",
					mgr->id);
		return;
	}

	// Do we define a new command for each of these, or use a generic?
	if (log)
		flags |= SESSION_CMDQ_FLAG_LOG;

	manager_site(mgr, path, flags);

}






//
// Required: SID, FROM or FID, TO
// Optional: LOG
//
void command2_cmd_ren(char **keys, char **values,
					  int items,void *optarg)
{
	command2_node_t *node = (command2_node_t *)optarg;
	manager_t *mgr;
	char *sid, *from, *fid, *to, *log;
	int flags = 0;

	// Required
	sid = parser_findkey(keys, values, items, "SID");
	from= parser_findkey(keys, values, items, "FROM");
	fid = parser_findkey(keys, values, items, "FID");
	to  = parser_findkey(keys, values, items, "TO");

	// Optional
	log = parser_findkey(keys, values, items, "LOG");

	if (!sid||!to) {
		lion_printf(node->handle,
					"REN|CODE=1501|Msg=Required arguments: SID, FROM or FID, TO.  (Optional: LOG.)\r\n");
		return;
	}


	// Find the session:
	if (!(mgr = manager_find_fromsid(atoi(sid)))) {
		lion_printf(node->handle,
					"REN|SID=%s|CODE=1506|MSG=No such SID defined.\r\n",
					sid);
		return;
	}

	// Do we own this?
	if (mgr->user != node) {
		lion_printf(node->handle,
					"REN|SID=%u|CODE=1508|MSG=You do not control the SID.\r\n",
					mgr->id);
		return;
	}

	if (from)
		from = misc_url_decode(from);
	to = misc_url_decode(to);

	if (!from && fid && *fid) {
		file_t *file;

		file = session_getfid(mgr->session, atoi(fid));

		if (!file) {
			lion_printf(node->handle,
						"REN|SID=%s|CODE=1515|MSG=No such FID in current cache: %s \r\n",
						sid, fid);
			return;
		}

		if (file) from = strdup(file->fullpath);

	}


	if (!from) {
		SAFE_FREE(to);
		lion_printf(node->handle,
					"REN|SID=%s|CODE=1501|Msg=Required arguments: SID, FROM or FID, TO.  (Optional: LOG.)\r\n", sid);
		return;
	}




	// Do we define a new command for each of these, or use a generic?
	if (log)
		flags |= SESSION_CMDQ_FLAG_LOG;

	manager_ren(mgr, from, to, flags);

	SAFE_FREE(from);
	SAFE_FREE(to);

}




//
// Required: SID, PATH or FID
// Optional: LOG
//
void command2_cmd_mdtm(char **keys, char **values,
					   int items,void *optarg)
{
	command2_node_t *node = (command2_node_t *)optarg;
	manager_t *mgr;
	char *sid, *path, *fid, *log;
	int flags = 0;

	// Required
	sid = parser_findkey(keys, values, items, "SID");
	path= parser_findkey(keys, values, items, "PATH");
	fid = parser_findkey(keys, values, items, "FID");

	// Optional
	log = parser_findkey(keys, values, items, "LOG");


	if (!sid) {
		lion_printf(node->handle,
					"MDTM|CODE=1501|Msg=Required arguments: SID, PATH or FID.  (Optional: LOG.)\r\n");
		return;
	}


	// Find the session:
	if (!(mgr = manager_find_fromsid(atoi(sid)))) {
		lion_printf(node->handle,
					"MDTM|SID=%s|CODE=1506|MSG=No such SID defined.\r\n",
					sid);
		return;
	}

	// Do we own this?
	if (mgr->user != node) {
		lion_printf(node->handle,
					"MDTM|SID=%u|CODE=1508|MSG=You do not control the SID.\r\n",
					mgr->id);
		return;
	}

	if (path)
		path = misc_url_decode(path);

	if (!path && fid && *fid) {
		file_t *file;

		file = session_getfid(mgr->session, atoi(fid));

		if (!file) {
			lion_printf(node->handle,
						"MDTM|SID=%s|CODE=1515|MSG=No such FID in current cache: %s \r\n",
						sid, fid);
			return;
		}

		if (file) path = strdup(file->fullpath);

	}


	if (!path) {

		lion_printf(node->handle,
					"MDTM|SID=%s|CODE=1501|Msg=Required arguments: SID, PATH or FID.  (Optional: LOG.)\r\n", sid);
		return;
	}


	// Do we define a new command for each of these, or use a generic?
	if (log)
		flags |= SESSION_CMDQ_FLAG_LOG;

	manager_mdtm(mgr, path, flags);

	SAFE_FREE(path);

}


//
// Required: QID
// Optional: TOGGLE
//
void command2_cmd_subscribe(char **keys, char **values,
							int items,void *optarg)
{
	command2_node_t *node = (command2_node_t *)optarg;
	char *qid, *toggle;
	queue_t *queue;


	qid     = parser_findkey(keys, values, items, "QID");

	if (!qid) {
		lion_printf(node->handle,
					"SUBSCRIBE|CODE=1501|Msg=Required arguments: QID. (Optionals: TOGGLE \r\n");
		return;
	}


	queue = queue_findbyqid(atoi(qid));

	if (!queue) {
		lion_printf(node->handle,
					"SUBSCRIBE|CODE=1512|MSG=No such QID defined: %d\r\n",
					atoi(qid));
		return;
	}


	toggle   = parser_findkey(keys, values, items, "TOGGLE");
	if (toggle && queue_is_subscriber(queue, node)) {

		queue_unsubscribe(queue, node);

		debugf("[subscribe] toggle off\n");

		lion_printf(node->handle,
					"SUBSCRIBE|QID=%s|CODE=0|UNSUBSCRIBED|MSG=Unsubscribed\r\n",
					qid);
		return;
	}

	queue_subscribe(queue, node);

	lion_printf(node->handle,
				"SUBSCRIBE|QID=%s|CODE=0|MSG=Subscribed\r\n",
				qid);

}


//
// Required: QID
// Optional:
//
void command2_cmd_unsubscribe(char **keys, char **values,
							  int items,void *optarg)
{
	command2_node_t *node = (command2_node_t *)optarg;
	char *qid;
	queue_t *queue;


	qid     = parser_findkey(keys, values, items, "QID");

	if (!qid) {
		lion_printf(node->handle,
					"UNSUBSCRIBE|CODE=1501|Msg=Required arguments: QID. (Optionals: TOGGLE \r\n");
		return;
	}


	queue = queue_findbyqid(atoi(qid));

	if (!queue) {
		lion_printf(node->handle,
					"UNSUBSCRIBE|CODE=1512|MSG=No such QID defined: %d\r\n",
					atoi(qid));
		return;
	}


	if (queue_is_subscriber(queue, node))
		queue_unsubscribe(queue, node);

	lion_printf(node->handle,
				"UNSUBSCRIBE|QID=%s|CODE=0|MSG=Unsubscribed\r\n",
				qid);

}

