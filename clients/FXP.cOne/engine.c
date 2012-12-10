#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <ncurses.h>
#include <cdk/cdk.h>


#include "lion.h"
#include "misc.h"

#include "engine.h"
#include "parser.h"
#include "file.h"
#include "main.h"
#include "display.h"
#include "qmgr.h"



lion_t *engine_handle = NULL;
extern FILE *d;
static char *engine_user = NULL;
static char *engine_pass = NULL;


static void engine_auth(void);


static unsigned int left_siteid  = 0;
static unsigned int right_siteid = 0;
static unsigned int left_sid     = 0;
static unsigned int right_sid    = 0;
static char        *left_name    = NULL;
static char        *right_name   = NULL;
static char        *engine_left_startdir  = NULL;
static char        *engine_right_startdir = NULL;

static unsigned int engine_qid   = 0;
static int          engine_queue_side = -1;

static int nested_site_calls    = 0;
static int nested_dele_calls    = 0;
static int nested_rmd_calls     = 0;
static int nested_rename_calls  = 0;
static int nested_qmove_calls   = 0;
static int nested_qdel_calls    = 0;

static int left_dirlist_locked  = 0;
static int right_dirlist_locked = 0;


void command_cmd_welcome    (char **, char **, int, void *);
void command_cmd_ssl        (char **, char **, int, void *);
void command_cmd_auth       (char **, char **, int, void *);
void command_cmd_sitelist   (char **, char **, int, void *);
void command_cmd_sessionnew (char **, char **, int, void *);
void command_cmd_connect    (char **, char **, int, void *);
void command_cmd_disconnect (char **, char **, int, void *);
void command_cmd_siteadd    (char **, char **, int, void *);
void command_cmd_sitemod    (char **, char **, int, void *);
void command_cmd_sitedel    (char **, char **, int, void *);
void command_cmd_site       (char **, char **, int, void *);
void command_cmd_dele       (char **, char **, int, void *);
void command_cmd_rmd        (char **, char **, int, void *);
void command_cmd_mkd        (char **, char **, int, void *);
void command_cmd_ren        (char **, char **, int, void *);
void command_cmd_qmove      (char **, char **, int, void *);
void command_cmd_subscribe  (char **, char **, int, void *);

void command_cmd_pwd        (char **, char **, int, void *);
void command_cmd_dirlist    (char **, char **, int, void *);
void command_cmd_queuenew   (char **, char **, int, void *);
void command_cmd_queuefree  (char **, char **, int, void *);
void command_cmd_qadd       (char **, char **, int, void *);
void command_cmd_qs         (char **, char **, int, void *);
void command_cmd_qc         (char **, char **, int, void *);
void command_cmd_qcompare   (char **, char **, int, void *);
void command_cmd_qget       (char **, char **, int, void *);
void command_cmd_qerr       (char **, char **, int, void *);
void command_cmd_qlist      (char **, char **, int, void *);
void command_cmd_qgrab      (char **, char **, int, void *);
void command_cmd_qdel       (char **, char **, int, void *);


static parser_command_list_t command_engine_list[] = {
	{ COMMAND( "WELCOME" ),   command_cmd_welcome   },
	{ COMMAND( "SSL"     ),   command_cmd_ssl       },
	{ COMMAND( "AUTH"    ),   command_cmd_auth      },
	{ COMMAND( "SITELIST"),   command_cmd_sitelist  },
	{ COMMAND( "SESSIONNEW"), command_cmd_sessionnew},
	{ COMMAND( "CONNECT" ),   command_cmd_connect   },
	{ COMMAND( "DISCONNECT" ),command_cmd_disconnect},
	{ COMMAND( "SITEADD" ),   command_cmd_siteadd   },
	{ COMMAND( "SITEMOD" ),   command_cmd_sitemod   },
	{ COMMAND( "SITEDEL" ),   command_cmd_sitedel   },
	{ COMMAND( "SITE"    ),   command_cmd_site      },
	{ COMMAND( "DELE"    ),   command_cmd_dele      },
	{ COMMAND( "RMD"     ),   command_cmd_rmd       },
	{ COMMAND( "MKD"     ),   command_cmd_mkd       },
	{ COMMAND( "REN"     ),   command_cmd_ren       },
	{ COMMAND( "QMOVE"   ),   command_cmd_qmove     },
	{ COMMAND( "SUBSCRIBE"),  command_cmd_subscribe },

	{ COMMAND( "PWD"     ),   command_cmd_pwd       },
	{ COMMAND( "DIRLIST" ),   command_cmd_dirlist   },
	{ COMMAND( "QUEUENEW"),   command_cmd_queuenew  },
	{ COMMAND( "QADD"    ),   command_cmd_qadd      },
	{ COMMAND( "QS"      ),   command_cmd_qs        },
	{ COMMAND( "QC"      ),   command_cmd_qc        },
	{ COMMAND( "QCOMPARE"),   command_cmd_qcompare  },
	{ COMMAND( "QGET"    ),   command_cmd_qget      },
	{ COMMAND( "QERR"    ),   command_cmd_qerr      },
	{ COMMAND( "QLIST"   ),   command_cmd_qlist     },
	{ COMMAND( "QGRAB"   ),   command_cmd_qgrab     },
	{ COMMAND( "QDEL"    ),   command_cmd_qdel      },
	{ NULL,  0         ,                    NULL }
};





int engine_handler(lion_t *handle, void *user_data, 
				   int status, int size, char *line)
{
	//	if (d) fprintf(d, "event %d\n", status);
	switch(status) {
	case LION_CONNECTION_CONNECTED:
		botbar_print("Connected.");
		break;

	case LION_CONNECTION_LOST:
		botbar_print("Connection failed: ");
		engine_handle = NULL;
		left_sid = 0;
		right_sid = 0;
		engine_qid = 0;
		engine_queue_side = -1;
		SAFE_FREE(left_name);
		SAFE_FREE(right_name);
		break;

	case LION_CONNECTION_CLOSED:
		botbar_print("Connection closed.");
		engine_handle = NULL;
		left_sid = 0;
		right_sid = 0;
		engine_qid = 0;
		engine_queue_side = -1;
		SAFE_FREE(left_name);
		SAFE_FREE(right_name);
		break;

	case LION_CONNECTION_SECURE_ENABLED:
		engine_auth();
		break;

	case LION_INPUT:
		//fprintf(d, ">> '%s'\n", line);
		parser_command(command_engine_list, line, user_data);
		break;

	}

	return 0;
}




int engine_init(void)
{

	if (lion_init())
		return 1;

	return 0;

}



void engine_free(void)
{

	SAFE_FREE(engine_user);
	SAFE_FREE(engine_pass);
	
	if (engine_handle) {

		if (engine_qid) {
			lion_printf(engine_handle, "QUEUEFREE|IFIDLE|QID=%u\r\n",
						engine_qid);
			engine_qid = 0;
		}

		lion_close(engine_handle);
	}

}



void engine_poll(void)
{

	// Basically, poll returns 1 for timeout, and 0 if there was data.
	// So, we could keep calling lion as long as we get 0.
	while (!lion_poll(100,0)) ;

}




void engine_connect(char *host, int port, char *user, char *pass)
{

	botbar_print("Connecting...");

	SAFE_DUPE(engine_user, user);

	SAFE_DUPE(engine_pass, pass);

	engine_handle = lion_connect(host, port,
								 0, 0, 
								 LION_FLAG_FULFILL,
								 NULL);
	lion_set_handler(engine_handle, engine_handler);


}




void engine_spawn(char *key, char *user, char *pass)
{




}


int lion_userinput(lion_t *handle, void *user_data, 
				   int status, int size, char *line)
{
	if (d) fprintf(d, "wrong %d\n", status);
	return 0;
}




static void engine_auth(void)
{

	lion_printf(engine_handle, "AUTH|USER=%s|PASS=%s\r\n",
				engine_user,
				engine_pass);

}


void engine_sitelist(void (*callback)(char **, char **, int))
{

	if (engine_handle) {
		lion_set_userdata(engine_handle, (void *)callback);
		lion_printf(engine_handle, "SITELIST\r\n");
	} else {
		callback(NULL, NULL, 0);
	}

}


void engine_qget(void (*callback)(char **, char **, int))
{

	if (engine_handle && (engine_qid > 0)) {
		lion_set_userdata(engine_handle, (void *)callback);
		lion_printf(engine_handle, "QGET|QID=%u\r\n", engine_qid);
	} else {
		callback(NULL, NULL, 0);
	}

}


void engine_qerr(void (*callback)(char **, char **, int))
{

	if (engine_handle && (engine_qid > 0)) {
		lion_set_userdata(engine_handle, (void *)callback);
		lion_printf(engine_handle, "QERR|QID=%u\r\n", engine_qid);
	} else {
		callback(NULL, NULL, 0);
	}

}



void engine_qlist(void (*callback)(char **, char **, int))
{

	if (engine_handle) {
		lion_set_userdata(engine_handle, (void *)callback);
		lion_printf(engine_handle, "QLIST\r\n");
	} else {
		callback(NULL, NULL, 0);
	}
	
}

void engine_qgrab(unsigned int qid, int sub, void (*callback)(int, char *))
{
	
	if (engine_handle) {
		lion_set_userdata(engine_handle, (void *)callback);
		lion_printf(engine_handle, "QGRAB|QID=%u%s\r\n", 
					qid ? qid : engine_qid,
					sub ? "|SUBSCRIBE" : "");
	} else {
		callback(0, "Not Connected");
	}
	
}


void engine_site(int side, int log, char *cmd, void (*callback)(char **, char **, int))
{
	unsigned int sid = 0;
	
	if (side == LEFT_SID)
		sid = left_sid;
	else if (side == RIGHT_SID)
		sid = right_sid;

	if (!sid) return; // Not connected.

	if (engine_handle) {
		nested_site_calls++;
		lion_set_userdata(engine_handle, (void *)callback);
		cmd = misc_url_encode(cmd);
		lion_printf(engine_handle, "SITE|SID=%u|%sCMD=%s\r\n", 
					sid, 
					log ? "LOG|" : "", 
					cmd);
		SAFE_FREE(cmd);
	} else {
		callback(NULL, NULL, 0);
	}

}




void engine_dele(int side, int log, char *cmd, void (*callback)(char **, char **, int))
{
	unsigned int sid = 0;
	
	if (side == LEFT_SID)
		sid = left_sid;
	else if (side == RIGHT_SID)
		sid = right_sid;

	if (!sid) return; // Not connected.

	if (engine_handle) {
		nested_dele_calls++;
		lion_set_userdata(engine_handle, (void *)callback);
		cmd = misc_url_encode(cmd);
		lion_printf(engine_handle, "DELE|SID=%u|%sPATH=%s\r\n", 
					sid, 
					log ? "LOG|" : "", 
					cmd);
		SAFE_FREE(cmd);
	} else {
		callback(NULL, NULL, 0);
	}
	
}


void engine_qmove(int from, int to, void (*callback)(char **, char **, int))
{

	if (engine_handle && (engine_qid > 0)) {
		nested_qmove_calls++;
		lion_set_userdata(engine_handle, (void *)callback);
		
		switch(to) {
		case -1: // TOP
			lion_printf(engine_handle, "QMOVE|QID=%u|FROM=%u|TO=FIRST\r\n",
						engine_qid, 
						from);
			break;
		case -2: // BOT
			lion_printf(engine_handle, "QMOVE|QID=%u|FROM=%u|TO=LAST\r\n",
						engine_qid, 
						from);
			break;
		default:
			lion_printf(engine_handle, "QMOVE|QID=%u|FROM=%u|TO=%u\r\n",
						engine_qid, 
						from,
						to);
			break;
		}
	} else {
		callback(NULL, NULL, 0);
	}
	
}



void engine_qdel(int at, void (*callback)(char **, char **, int))
{

	if (engine_handle && (engine_qid > 0)) {

		nested_qdel_calls++;
		lion_set_userdata(engine_handle, (void *)callback);
		
		lion_printf(engine_handle, "QDEL|QID=%u|@=%u\r\n",
					engine_qid, 
					at);

	} else {
		callback(NULL, NULL, 0);
	}
	
}


void engine_rmd(int side, int log, char *cmd, void (*callback)(char **, char **, int))
{
	unsigned int sid = 0;
	
	if (side == LEFT_SID)
		sid = left_sid;
	else if (side == RIGHT_SID)
		sid = right_sid;

	if (!sid) return; // Not connected.

	if (engine_handle) {
		nested_rmd_calls++;
		lion_set_userdata(engine_handle, (void *)callback);
		cmd = misc_url_encode(cmd);
		lion_printf(engine_handle, "RMD|SID=%u|%sPATH=%s\r\n", 
					sid, 
					log ? "LOG|" : "", 
					cmd);
		SAFE_FREE(cmd);
	} else {
		callback(NULL, NULL, 0);
	}
	
}

void engine_mkd(int side, int log, char *cmd, void (*callback)(char **, char **, int))
{
	unsigned int sid = 0;
	
	if (side == LEFT_SID)
		sid = left_sid;
	else if (side == RIGHT_SID)
		sid = right_sid;

	if (!sid) return; // Not connected.

	if (engine_handle) {
		lion_set_userdata(engine_handle, (void *)callback);
		cmd = misc_url_encode(cmd);
		lion_printf(engine_handle, "MKD|SID=%u|%sPATH=%s\r\n", 
					sid, 
					log ? "LOG|" : "", 
					cmd);
		SAFE_FREE(cmd);
	} else {
		callback(NULL, NULL, 0);
	}
	
}




void engine_rename(int side, int log, char *from, char *to, void (*callback)(char **, char **, int))
{
	unsigned int sid = 0;
	
	if (side == LEFT_SID)
		sid = left_sid;
	else if (side == RIGHT_SID)
		sid = right_sid;

	if (!sid) return; // Not connected.

	if (engine_handle) {
		nested_rename_calls++;
		lion_set_userdata(engine_handle, (void *)callback);
		from = misc_url_encode(from);
		to   = misc_url_encode(to);
		lion_printf(engine_handle, "REN|SID=%u|%sFROM=%s|TO=%s\r\n", 
					sid, 
					log ? "LOG|" : "", 
					from,
					to);
		SAFE_FREE(from);
		SAFE_FREE(to);
	} else {
		callback(NULL, NULL, 0);
	}
	
}





//
// Connect to a site. If we are already connected, release it first.
// If we have a QID, release that now, use would have to go via qlist
// to view a queue from here.
//
void engine_connect_left(unsigned int siteid, char *name)
{

	if (left_sid)
		lion_printf(engine_handle, "SESSIONFREE|SID=%u\r\n",
					left_sid);

	left_sid = 0;
	left_siteid = siteid;
	SAFE_COPY(left_name, name);

	if (engine_qid) {
		lion_printf(engine_handle, "QUEUEFREE|QID=%u|IFIDLE\r\n",
					engine_qid);
		engine_qid = 0;
	}


	lion_printf(engine_handle, "SESSIONNEW|SITEID=%u\r\n",
				siteid);

}


void engine_connect_right(unsigned int siteid, char *name)
{

	if (right_sid)
		lion_printf(engine_handle, "SESSIONFREE|SID=%u\r\n",
					right_sid);

	right_sid = 0;
	right_siteid = siteid;
	SAFE_COPY(right_name, name);

	if (engine_qid) {
		lion_printf(engine_handle, "QUEUEFREE|QID=%u|IFIDLE\r\n",
					engine_qid);
		engine_qid = 0;
	}

	lion_printf(engine_handle, "SESSIONNEW|SITEID=%u\r\n",
				siteid);

}




void engine_pwd(int side)
{

	if ((side == LEFT_SID) &&
		(left_sid)) {

		if (left_dirlist_locked) return;
		left_dirlist_locked = 1;

		lion_printf(engine_handle, "PWD|SID=%u\r\n",
					left_sid);
	}

	if ((side == RIGHT_SID) &&
		(right_sid)) {

		if (right_dirlist_locked) return;
		right_dirlist_locked = 1;

		lion_printf(engine_handle, "PWD|SID=%u\r\n",
					right_sid);
	}

}




void engine_cwd(int side, char *path)
{

	path = misc_url_encode(path);

	if ((side == LEFT_SID) &&
		(left_sid)) {
		lion_printf(engine_handle, "CWD|SID=%u|PATH=%s\r\n",
					left_sid, path);
	}

	if ((side == RIGHT_SID) &&
		(right_sid)) {
		lion_printf(engine_handle, "CWD|SID=%u|PATH=%s\r\n",
					right_sid, path);
	}

	SAFE_FREE(path);

	engine_pwd(side);
}




int engine_queue(int side, unsigned int fid)
{

	// Do we have a QID?
	if (!engine_qid) {
		// Do we have two sids?
		if (!left_sid || !right_sid) return 0;

		if (engine_queue_side == -1) {
			engine_queue_side = side;
			lion_printf(engine_handle, 
						"QUEUENEW|NORTH_SID=%u|SOUTH_SID=%u\r\n",
						left_sid, right_sid);
		}

		return 0;
	}


	lion_printf(engine_handle, "QADD|QID=%u|SRC=%s|FID=%u\r\n",
			   engine_qid,
			   side == LEFT_SID ? "NORTH" : "SOUTH",
			   fid);

	return 1;
}



void engine_subscribe(unsigned int qid)
{

	lion_printf(engine_handle, "SUBSCRIBE|QID=%u|TOGGLE\r\n",
				qid);

}



int engine_go(void)
{

	if (engine_qid) {

		lion_printf(engine_handle, "GO|QID=%u|SUBSCRIBE\r\n",
					engine_qid);
		left_sid = 0;
		right_sid = 0;

		//SAFE_FREE(left_name);
		//SAFE_FREE(right_name);

		return 1;
	}

	return 0;
}





int engine_isconnected(int side)
{

	if (side == LEFT_SID) {
		if (left_sid > 0) return 1;
	} 
	if (right_sid > 0) return 1;

	return 0;
}


int engine_hasqueue(void)
{
	
	if (engine_qid > 0) return 1;

	return 0;
}


void engine_compare(void)
{

	if (!engine_qid) {
		// Do we have two sids?
		if (!left_sid || !right_sid) return;

		if (engine_queue_side == -1) {
			engine_queue_side = -1;  // Side is -1, we mean COMPARE
			lion_printf(engine_handle, 
						"QUEUENEW|NORTH_SID=%u|SOUTH_SID=%u\r\n",
						left_sid, right_sid);
		}

		return;
	}

	lion_printf(engine_handle, "QCOMPARE|QID=%u\r\n", engine_qid);

}


unsigned int engine_current_qid(void)
{
	return engine_qid;
}


void engine_set_sitename(int side, char *name)
{

	if (side == LEFT_SID) {
		SAFE_COPY(left_name, name);
	} else {
		SAFE_COPY(right_name, name);
	} 
}

char *engine_get_sitename(int side)
{

	if (side == LEFT_SID)
		return left_name ? left_name : "left-side";
	else
		return right_name ? right_name : "right-side";
}


void engine_set_startdir (int side, char *dir)
{

	if (side == LEFT_SID) {
		SAFE_COPY(engine_left_startdir, dir);
	} else {
		SAFE_COPY(engine_right_startdir, dir);
	} 
}

char *engine_get_startdir (int side)
{

	if (side == LEFT_SID) {
		return engine_left_startdir;
	} else {
		return engine_right_startdir;
	} 
	return NULL;
}



void engine_queuefree(unsigned int qid)
{

	lion_printf(engine_handle, "QUEUEFREE|QID=%u\r\n",
				qid);

}













void command_cmd_welcome(char **keys, char **values, int items, void *optarg)
{
	char *version;
	char *ssl;
	int use_ssl;

	botbar_print("FXP.One engine says Welcome.");

	ssl     = parser_findkey(keys, values, items, "SSL");
	version = parser_findkey(keys, values, items, "VERSION");

	// Check version?

	// Can we ssl?
	if (ssl && !mystrccmp("disabled", ssl))
		use_ssl = 0;
	else
		use_ssl = 1;

	if (use_ssl)
		lion_printf(engine_handle, "SSL\r\n");
	else
		engine_auth();

}




void command_cmd_ssl(char **keys, char **values, int items, void *optarg)
{
	char *code;

	code = parser_findkey(keys, values, items, "CODE");

	if (code && atoi(code)==0)
		lion_ssl_set(engine_handle, LION_SSL_CLIENT);
	else
		engine_auth();

}




void command_cmd_auth(char **keys, char **values, int items, void *optarg)
{
	char *code;
	
	code = parser_findkey(keys, values, items, "CODE");

	if (code && !atoi(code)) {
		botbar_print("Authenticated with FXP.One successfully");
		main_connected();
	} else {
		botbar_print("Authentication failed");
		lion_close(engine_handle);
		engine_handle = NULL;
	}

}




void command_cmd_sitelist(char **keys, char **values, int items, void *optarg)
{
	void (*callback)(char **, char **, int);
	char *end;

	callback = (void (*)(char **, char **, int))optarg;

	end = parser_findkey(keys, values, items, "END");
				
	if (end) {
		//		engine_gofast = 0;
		lion_set_userdata(engine_handle, NULL);
		callback(NULL, NULL, 0);
	} else {
		// engine_gofast = 1;
		callback(keys, values, items);
	}
	
}



void command_cmd_sessionnew(char **keys, char **values, int items, 
							void *optarg)
{
	char *siteid;
	char *sid;
	unsigned n;

	siteid = parser_findkey(keys, values, items, "SITEID");
	sid    = parser_findkey(keys, values, items, "SID");
				
	if (siteid && sid) {
		
		n = atoi(siteid);
		if ((left_siteid == n) && !left_sid) {
			left_sid = atoi(sid);
			return;
		}
		if ((right_siteid == n)) {
			right_sid = atoi(sid);
			return;
		}
		if ((left_siteid == n)) {
			left_sid = atoi(sid);
			return;
		}
	}
}


void command_cmd_connect(char **keys, char **values, int items, 
						 void *optarg)
{
	char *sid;
	int side = 0;

	if (!(sid    = parser_findkey(keys, values, items, "SID"))) return;

	if (atoi(sid) == right_sid)
		side = RIGHT_SID;
	else if (atoi(sid) == left_sid) 
		side = LEFT_SID;
	else return;

	main_site_connected(side);

}


void command_cmd_disconnect(char **keys, char **values, int items, 
						 void *optarg)
{
	char *sid, *msg, *code;
	int right = 0;

	if (!(sid    = parser_findkey(keys, values, items, "SID"))) return;

	// Use msg for english, or code for locailsed.
	msg  = parser_findkey(keys, values, items, "MSG");
	code = parser_findkey(keys, values, items, "CODE");

	if (atoi(sid) == right_sid)
		right = RIGHT_SID;
	else if (atoi(sid) != left_sid) return;
	
	if (!right) {
		left_siteid = 0;
		left_sid = 0;
	} else {
		right_siteid = 0;
		right_sid = 0;
	}
	
	// Tell someone!
	if (!msg)
		msg = "Failed: Unknown reason";

	display_set_files(right, msg);
	
	botbar_printf("%s: failed code (%s): %s", 
				  engine_get_sitename(right),
				  code ? code : "",
				  msg);

}


void command_cmd_siteadd(char **keys, char **values, int items, 
						 void *optarg)
{
	char *code, *msg;

	code  = parser_findkey(keys, values, items, "CODE");
	msg   = parser_findkey(keys, values, items, "MSG");

	if (code && !atoi(code)) {
		botbar_print("Added site successfully.");
		return;
	}

	botbar_printf("Add site failed (%s): %s",
				  code ? code : "",
				  msg ? msg : "unknown failure");

}


void command_cmd_sitemod(char **keys, char **values, int items, 
						  void *optarg)
{
	char *code, *msg;

	code  = parser_findkey(keys, values, items, "CODE");
	msg   = parser_findkey(keys, values, items, "MSG");

	if (code && !atoi(code)) {
		botbar_print("Modified site successfully.");
		return;
	}

	botbar_printf("Edit site failed (%s): %s",
				  code ? code : "",
				  msg ? msg : "unknown failure");

}



void command_cmd_sitedel(char **keys, char **values, int items, 
						  void *optarg)
{
	char *code, *msg;

	code  = parser_findkey(keys, values, items, "CODE");
	msg   = parser_findkey(keys, values, items, "MSG");

	if (code && !atoi(code)) {
		botbar_print("Deleted site successfully.");
		return;
	}

	botbar_printf("Delete site failed (%s): %s",
				  code ? code : "",
				  msg ? msg : "unknown failure");

}






void command_cmd_pwd(char **keys, char **values, int items, 
						  void *optarg)
{
	char *sid, *path;

	sid   = parser_findkey(keys, values, items, "SID");
	path  = parser_findkey(keys, values, items, "PATH");

	if (!sid || !path) {
		return;
	}

	path = misc_url_decode(path);

	if (left_sid && (atoi(sid) == left_sid)) {
		display_set_lpath(path);
		lion_printf(engine_handle, "DIRLIST|SID=%u\r\n", left_sid);
	} else if (right_sid && (atoi(sid) == right_sid)) {
		display_set_rpath(path);
		lion_printf(engine_handle, "DIRLIST|SID=%u\r\n", right_sid);
	} else {
		botbar_printf("Message for unknown SID: %s\n", sid);
	}

	SAFE_FREE(path);
}



void command_cmd_dirlist(char **keys, char **values, int items, 
						 void *optarg)
{
	char *sid;
	file_t *file;
	int side;
	char *name, *fid, *date, *size, *type, *user, *group, *perm, *end;

	sid   = parser_findkey(keys, values, items, "SID");

	if (!sid) return;

	// Check SID is for us.
	if (left_sid && (atoi(sid) == left_sid)) {
		side = LEFT_SID;
	} else if (right_sid && (atoi(sid) == right_sid)) {
		side = RIGHT_SID;
	} else {
		botbar_printf("Message for unknown SID: %s\n", sid);
		return;
	}

	end    = parser_findkey(keys, values, items, "END");

	if (end) {

		// Call inject with empty/dunno node. IF we had not
		// already called inject, this will ensure ".." is added.
		display_inject_file(side, NULL);

		display_sort_files(side, 0);

		if (side == LEFT_SID)
			left_dirlist_locked = 0;
		else if (side == RIGHT_SID)
			right_dirlist_locked = 0;

		return;
	}

	
	fid    = parser_findkey(keys, values, items, "FID");
	name   = parser_findkey(keys, values, items, "NAME");
	user   = parser_findkey(keys, values, items, "USER");
	group  = parser_findkey(keys, values, items, "GROUP");
	perm   = parser_findkey(keys, values, items, "PERM");
	date   = parser_findkey(keys, values, items, "DATE");
	size   = parser_findkey(keys, values, items, "SIZE");
	type   = parser_findkey(keys, values, items, "FTYPE");

	if (!fid||!name||!user||!group||!date||!size||!type)
		return;
	
	file = file_new();
	if (!file) return;

	// Set the name in colour.
	file_strdup_name(file, name, type);
	file->user  = strdup(user);
	file->group = strdup(group);
	file->perm  = strdup(perm);
	file->fid   = strtoul(fid, NULL, 10);
	file->date  = strtoul(date, NULL, 10);
	file->size  = strtoull(size, NULL, 10);

	display_inject_file(side, file);

}




void command_cmd_queuenew(char **keys, char **values, int items, 
						  void *optarg)
{
	char *code, *qid;

	code = parser_findkey(keys, values, items, "CODE");
	qid  = parser_findkey(keys, values, items, "QID");

	if (!qid || !code) return;

	// We have a new queue
	engine_qid = strtoul(qid, NULL, 10);

	if (engine_queue_side == -1) { // COMAPRE
		engine_compare();
		return;
	}

	if (engine_queue_side == LEFT_SID) {
		engine_queue_side = -1;
		display_queue_selected(LEFT_SID);
	}

	if (engine_queue_side == RIGHT_SID) {
		engine_queue_side = -1;
		display_queue_selected(RIGHT_SID);
	}

}


void command_cmd_queuefree(char **keys, char **values, int items, 
						   void *optarg)
{
	char *qid;

	qid  = parser_findkey(keys, values, items, "QID");

	if (!qid) return;

	// We have a new queue
	if (engine_qid == strtoul(qid, NULL, 10))
		engine_qid = 0;

	// What about the SIDs? GO, they should be cleared. So
	// should be ok to ignore.

}



void command_cmd_qadd(char **keys, char **values, int items, 
						  void *optarg)
{
	char *code, *qid, *fid, *src;
	int side;
	unsigned int value;

	code = parser_findkey(keys, values, items, "CODE");
	qid  = parser_findkey(keys, values, items, "QID");
	src  = parser_findkey(keys, values, items, "SRC");
	fid  = parser_findkey(keys, values, items, "FID");

	if (!qid || !code || !fid || !src) return;

	if (!mystrccmp("NORTH", src))
		side = LEFT_SID;
	else if (!mystrccmp("SOUTH", src))
		side = RIGHT_SID;
	else return;

	value = strtoul(qid, NULL, 10);

	if (value != engine_qid) return ;

	value = strtoul(fid, NULL, 10);

	display_queued_item(side, value);

}




void command_cmd_qs(char **keys, char **values, int items, 
					void *optarg)
{
	char *qid, *msg;
	unsigned int value;

	qid  = parser_findkey(keys, values, items, "QID");
	msg  = parser_findkey(keys, values, items, "type");

	if (!qid) return;

	value = strtoul(qid, NULL, 10);

	// We allow QS from any QID for display purposes.
	//if (value != engine_qid) return ;

	qmgr_qs(keys, values, items);

}



void command_cmd_qc(char **keys, char **values, int items, 
					void *optarg)
{
	char *qid, *msg;
	unsigned int value;
	
	qid  = parser_findkey(keys, values, items, "QID");
	msg  = parser_findkey(keys, values, items, "type");
	
	if (!qid) return;

	value = strtoul(qid, NULL, 10);

	//if (value != engine_qid) return ;

	qmgr_qc(keys, values, items);

}


void command_cmd_subscribe(char **keys, char **values, int items, 
						   void *optarg)
{
	char *qid, *msg;
	
	qid  = parser_findkey(keys, values, items, "QID");
	msg  = parser_findkey(keys, values, items, "UNSUBSCRIBED");
	
	if (!qid) return;

	qmgr_printf("%s to queue (%s).",
				msg ? "Unsubscribed" : "Subscribed",
				qid);

}



void command_cmd_qcompare(char **keys, char **values, int items, 
						  void *optarg)
{
	char *qid, *north, *south, *end;
	unsigned int value;

	qid  = parser_findkey(keys, values, items, "QID");

	if (!qid) return;
	value = strtoul(qid, NULL, 10);
	if (value != engine_qid) return ;

	north  = parser_findkey(keys, values, items, "NORTH_FID");
	if (north)
		display_select_fids(LEFT_SID, north);

	south  = parser_findkey(keys, values, items, "SOUTH_FID");
	if (south)
		display_select_fids(RIGHT_SID, south);

	end  = parser_findkey(keys, values, items, "END");
	if (end)
		display_select_fids(RIGHT_SID, NULL);
		
}



void command_cmd_qget(char **keys, char **values, int items, void *optarg)
{
	void (*callback)(char **, char **, int);
	char *end, *qid;
	unsigned int value;

	callback = (void (*)(char **, char **, int))optarg;

	qid  = parser_findkey(keys, values, items, "QID");

	if (!qid) return;
	value = strtoul(qid, NULL, 10);
	if (value != engine_qid) return ;

	end = parser_findkey(keys, values, items, "END");
				
	if (end) {
		lion_set_userdata(engine_handle, NULL);
		callback(NULL, NULL, 0);
	} else {
		callback(keys, values, items);
	}
	
}


void command_cmd_qerr(char **keys, char **values, int items, void *optarg)
{
	void (*callback)(char **, char **, int);
	char *end, *qid;
	unsigned int value;

	callback = (void (*)(char **, char **, int))optarg;

	qid  = parser_findkey(keys, values, items, "QID");

	if (!qid) return;
	value = strtoul(qid, NULL, 10);
	if (value != engine_qid) return ;

	end = parser_findkey(keys, values, items, "END");
				
	if (end) {
		lion_set_userdata(engine_handle, NULL);
		callback(NULL, NULL, 0);
	} else {
		callback(keys, values, items);
	}
	
}



void command_cmd_qlist(char **keys, char **values, int items, void *optarg)
{
	void (*callback)(char **, char **, int);
	char *end;

	callback = (void (*)(char **, char **, int))optarg;

	end = parser_findkey(keys, values, items, "END");
				
	if (end) {
		lion_set_userdata(engine_handle, NULL);
		if (callback)
			callback(NULL, NULL, 0);
	} else {
		if (callback)
			callback(keys, values, items);
	}
	
}


void command_cmd_site(char **keys, char **values, int items, void *optarg)
{
	void (*callback)(char **, char **, int);
	char *code;
	
	callback = (void (*)(char **, char **, int))optarg;

	code = parser_findkey(keys, values, items, "CODE");
				
	// If we get CODE, the command is over, otherwise its LOG output
	if (code && atoi(code) != -1) {
		nested_site_calls--;
		if (!nested_site_calls)
			lion_set_userdata(engine_handle, NULL);
	}

	if (callback)
		callback(keys, values, items);
	
}


void command_cmd_dele(char **keys, char **values, int items, void *optarg)
{
	void (*callback)(char **, char **, int);
	char *code;
	
	callback = (void (*)(char **, char **, int))optarg;

	code = parser_findkey(keys, values, items, "CODE");
				
	// If we get CODE, the command is over, otherwise its LOG output
	if (code && atoi(code) != -1) {
		nested_dele_calls--;
		if (!nested_dele_calls)
			lion_set_userdata(engine_handle, NULL);
	}

	if (callback)
		callback(keys, values, items);
	
}

void command_cmd_rmd(char **keys, char **values, int items, void *optarg)
{
	void (*callback)(char **, char **, int);
	char *code;
	
	callback = (void (*)(char **, char **, int))optarg;

	code = parser_findkey(keys, values, items, "CODE");
				
	// If we get CODE, the command is over, otherwise its LOG output
	if (code && atoi(code) != -1) {
		nested_rmd_calls--;
		if (!nested_rmd_calls)
			lion_set_userdata(engine_handle, NULL);
	}

	if (callback)
		callback(keys, values, items);
	
}



void command_cmd_mkd(char **keys, char **values, int items, void *optarg)
{
	void (*callback)(char **, char **, int);
	char *code;
	
	callback = (void (*)(char **, char **, int))optarg;

	code = parser_findkey(keys, values, items, "CODE");
				
	// If we get CODE, the command is over, otherwise its LOG output
	if (code && atoi(code) != -1) {
		lion_set_userdata(engine_handle, NULL);
	}

	if (callback)
		callback(keys, values, items);
	
}



void command_cmd_ren(char **keys, char **values, int items, void *optarg)
{
	void (*callback)(char **, char **, int);
	char *code;
	
	callback = (void (*)(char **, char **, int))optarg;

	code = parser_findkey(keys, values, items, "CODE");
				
	// If we get CODE, the command is over, otherwise its LOG output
	if (code && atoi(code) != -1) {
		nested_rename_calls--;
		if (!nested_rename_calls)
			lion_set_userdata(engine_handle, NULL);
	}

	if (callback)
		callback(keys, values, items);
	
}


void command_cmd_qmove(char **keys, char **values, int items, void *optarg)
{
	void (*callback)(char **, char **, int);
	char *code;
	
	callback = (void (*)(char **, char **, int))optarg;

	code = parser_findkey(keys, values, items, "CODE");
				
	// If we get CODE, the command is over, otherwise its LOG output
	if (code && atoi(code) != -1) {
		nested_qmove_calls--;
		if (!nested_qmove_calls)
			lion_set_userdata(engine_handle, NULL);
	}

	if (callback)
		callback(keys, values, items);
	
}



void command_cmd_qdel(char **keys, char **values, int items, void *optarg)
{
	void (*callback)(char **, char **, int);
	char *code;
	
	callback = (void (*)(char **, char **, int))optarg;

	code = parser_findkey(keys, values, items, "CODE");
				
	// If we get CODE, the command is over, otherwise its LOG output
	if (code && atoi(code) != -1) {
		nested_qdel_calls--;
		if (!nested_qdel_calls)
			lion_set_userdata(engine_handle, NULL);
	}

	if (callback)
		callback(keys, values, items);
	
}



void command_cmd_qgrab(char **keys, char **values, int items, void *optarg)
{
	void (*callback)(int, char *);
	char *code, *qid, *north, *south, *msg, *n_siteid, *s_siteid, *sub;
	char *n_name, *s_name;

	// QGRAB|QID=1|CODE=0|ITEMS=0|NORTH_SID=34|SOUTH_SID=35|NORTH_SITEID=0|SOUTH_SITEID=4
	callback = (void (*)(int, char *))optarg;
	lion_set_userdata(engine_handle, NULL);

	//fprintf(d, "QGRAB reply '%s'\n", parser_buildstring(keys, values, items));


	
	code  = parser_findkey(keys, values, items, "CODE");
	sub   = parser_findkey(keys, values, items, "SUBSCRIBE");
	qid   = parser_findkey(keys, values, items, "QID");
	north = parser_findkey(keys, values, items, "NORTH_SID");
	south = parser_findkey(keys, values, items, "SOUTH_SID");
	msg   = parser_findkey(keys, values, items, "MSG");
	n_siteid = parser_findkey(keys, values, items, "NORTH_SITEID");
	s_siteid = parser_findkey(keys, values, items, "SOUTH_SITEID");
	n_name = parser_findkey(keys, values, items, "NORTH_NAME");
	s_name = parser_findkey(keys, values, items, "SOUTH_NAME");

	if (!code || !qid || !n_siteid || !s_siteid) {

		if (sub) {
			engine_qid = strtoul(qid, NULL, 10);
			callback(2, NULL); // 2 is subscribed
		} else
			callback(0, msg ? msg : "unknown reason");

		return;

	}

	// Did we get any sids? Basically, if we got TWO sids, we 
	// keep the QID and go to DISPLAY.
	// Otherwise, we free the QID, keep the one SID, and SESSIONNEW the other.

	// Only one side connected, free queue. Both, keep.
	if (!north || !south)
		engine_queuefree(strtoul(qid, NULL, 10));
	else 
		engine_qid = strtoul(qid, NULL, 10);
		

	if (north)
		left_sid = strtoul(north, NULL, 10);
	else 
		engine_connect_left(strtoul(n_siteid, NULL, 10),
							"From QGRAB");
	
	if (south)
		right_sid = strtoul(south, NULL, 10);
	else
		engine_connect_right(strtoul(s_siteid, NULL, 10),
							 "From QGRAB");

	engine_set_sitename(LEFT_SID,  n_name);
	engine_set_sitename(RIGHT_SID, s_name);

	callback(1, NULL); // 1 is qgrab OK

}
