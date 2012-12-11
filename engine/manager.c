// $Id: manager.c,v 1.20 2010/10/12 02:36:32 lundman Exp $
//
// Jorgen Lundman 18th March 2004.
//
//
//


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#if __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif

#include "lion.h"
#include "misc.h"

#include "debug.h"
#include "command2.h"
#include "manager.h"
#include "queue.h"

#include "session.h"
#include "handler.h"

//#include "data.h"
//#include "sites.h"

//#include "engine.h"



/*
 *
 * The responsibility of the queue manager is to be one level higher than
 * the session handler. This is the only entity that will handle more than
 * one session at a time. (typically, 2). It deals with locking sessions
 * when they are in use by the manager, as well as relaying information from
 * the session handler to any user that may or may not be connected.
 *
 * It should handle such commands as:
 *
 * sessionlist    - list existing "queues". (Which may have a session)
 * sessionnew     - gets you a new session, and "queue" with it.
 * sessiontake    - take ownership of "queue". (Allow queue modifications)
 * sessionfree    - release
 * sessionsubscribe - Request log and stats to be sent automatically/contig.
 * sessioninfo    - Request one off status
 *
 * queueget
 * queueinsert
 * queueremove
 * queuemove [1]
 * queuestart
 * queuestop
 * queueabort [2]
 *
 * [1] move is the same as insert, and remove. But it is nice to be able to
 * do the action as a single command.
 *
 * [2] abort would be using the ABORT RFC command. On some FTPD's (most
 * notably that of "glftpd" it will also drop the control session and
 * therefor drop the "session". This is incorrect behavior, so users should
 * be aware of this and use it sparingly.
 *
 */

// We will assume one queue only. This means the GUI clients only use one
// method for issuing commands. However, the engine will act slightly
// differently based on if the session is idle or not. If it is idle, we
// will insert the request at the top of the queue, then add a "stop" queue
// command, and activate the queue.

//
// Hmm when you issue DIRLIST you can also issue AUTOQUEUE=1 if it is ok
// to automatically queue it. If 0, return error on not-idle.







//
// Our list of manager nodes.
//
static manager_t *manager_head = NULL;

// Current node id.
static unsigned int manager_id = 1; // 0 means delete node.






//
// global init function. run once on code startup.
//
int manager_init(void)
{

	return 0;

}


//
// global free function. run once on code exit.
//
void manager_free(void)
{
	manager_t *runner, *next = NULL;

	for (runner = manager_head; runner; runner = next) {
		next = runner->next;

        runner->next = NULL;
        // manager_release may call to iterate manager_head
        if (runner == manager_head) manager_head = NULL;

		manager_release(runner);
		SAFE_FREE(runner);

	}

	manager_head = NULL;

}



//
// We have been asked for a new queue/session/site
manager_t *manager_new(command2_node_t *user, sites_t *site, int flags)
{
	manager_t *node;

	node = (manager_t *) malloc(sizeof(*node));

	if (!node) {
        if (user && user->handle)
            lion_printf(user->handle,"SESSIONNEW|CODE=1401|MSG=Out of memory.\r\n");
		return NULL;
	}

	memset(node, 0, sizeof(*node));


	node->next = manager_head;
	manager_head=node;


	// This link/reference needs to be removed when a user disconnects.
	//node->user = user;
	manager_set_user(node, user);

	node->user_flags = flags;

	node->site = site;

	node->id = manager_id++;

    if (user && user->handle)
        lion_printf(user->handle, "SESSIONNEW|SITEID=%u|CODE=0|SID=%u\r\n",
                    site->id, node->id);

	// create a new session setting up a handler.
	node->session = session_new(handler_withuser, site);

	debugf("[manager] created new mgr %p id %d \n",
		   node, node->id);

	return node;
}


//
// Release a session, this is only called once the decision has been
// made to remove it entirely.
//
void manager_release(manager_t *node)
{

	debugf("[manager] release %p\n", node);

	// Some queues might refer to us, so we need to make sure that's not an
	// issue. Normally managers with queues are only released when the queue
	// say so. (and then ->queue is NULL)
	if (node->qid) {
		debugf("[manager] WARNING - release called when controlled by queue?\n");
	}


	if (node->user) {
		node->user = NULL;
	}

	if (node->session) {

		node->old_session = node->session;
		node->session = NULL;
		session_setclose(node->old_session, "Controlling Manager released");

	}

	node->id  = 0;
	node->qid = 0;
	node->site = NULL;


}


command2_node_t *manager_get_user(manager_t *node)
{

	return node->user;

}

void manager_set_user(manager_t *node, command2_node_t *user)
{

	node->user = user;
	node->connected = 0;

}

void manager_unlink_user(manager_t *node)
{

	if (node)
		node->user = NULL;

}


void manager_unlink_all_user(command2_node_t *user)
{
	manager_t *runner;

	for (runner = manager_head; runner; runner=runner->next) {
		if (runner->user == user) {

			manager_unlink_user(runner);

			// If we don't belong to a queue, release manager
			if (!runner->qid)
				manager_release(runner);

		}
	}
}





void manager_cleanup(void)
{
	manager_t *node, *prev;

	for( node = manager_head, prev = NULL;
		 node;
		 prev = node, node=node->next) {

		// Old session?
		if (node->old_session) {
			debugf("[manager] Cleaning up old session: %p\n",
				   node->old_session);

			if (node->old_session->handle)
			  lion_set_userdata(node->old_session->handle, NULL);
			if (node->old_session->data_handle)
			  lion_set_userdata(node->old_session->data_handle,
					    NULL);

			session_free(node->old_session);
			node->old_session = NULL;
			node->session = NULL;
		}



		if (node->id == 0) { // Release node.

			if (!prev)
				manager_head = node->next;
			else
				prev->next = node->next;

			debugf("[manager] releasing %p\n", node);
			SAFE_FREE(node);
			return; // only one at a time.
		}

	}

}









manager_t *manager_find_fromsession(session_t *session)
{
	manager_t *runner;

	for (runner = manager_head; runner; runner=runner->next) {
		if (runner->session == session)
			return runner;

	}

	return NULL;
}


manager_t *manager_find_fromsid(unsigned int sid)
{
	manager_t *runner;

	for (runner = manager_head; runner; runner=runner->next) {
		if (runner->id == sid) {
			return runner;
		}
	}

	return NULL;
}



void manager_dirlist(manager_t *mgr, char *args, char *path, int flags)
{
	lion_t *huser = NULL;
	char *combined = NULL;
	// Check mgr is valid?
	// Check session is valid?
	// Check session is idle?
	// issue dirlist.


	if (mgr && mgr->user && mgr->user->handle)
		huser = mgr->user->handle;


	if (!mgr->session) {

		lion_printf(huser, "DIRLIST|SID=%u|CODE=1507|MSG=No active session.\r\n",
					mgr->id);
		return;
	}

	// Is cached ok? and we have a cache..
	if ((flags & SESSION_CMDQ_FLAG_CACHEOK) &&
		 mgr->session->files &&
		 (mgr->session->files_used > 0)) {

		// If so, just send event like we've finished a dirlist.
		mgr->session->handler(mgr->session,
						 SESSION_EVENT_CMD,
						 mgr->session->files_reply_id,
						 mgr->session->files_used,
						 (char *)mgr->session->files );

		return;
	}


	// If path was given, we need to URL decode it.
	if (path)
		path = misc_url_decode(path);


	// add path and args together, if both are set.
	if (args && path &&
		*args && *path) {

		combined = (char *) malloc(strlen(args) + strlen(path) + 1 + 1);
		//                                                      spc nul
		if (combined) {
			strcpy(combined, args);
			strcat(combined, " ");
			strcat(combined, path);
		}

	}


	session_dirlist(mgr->session,
					combined ? combined :
					args ? args : path,
					flags,
					HANDLER_DIRLIST_SENDTO_USER);

	SAFE_FREE(combined);

	// If path was set, we url_decoded it, and hence need to free.
	SAFE_FREE(path);
}


void manager_quote(manager_t *mgr, char *args, int flags)
{
	lion_t *huser = NULL;
	// Check mgr is valid?
	// Check session is valid?
	// Check session is idle?

	if (mgr && mgr->user && mgr->user->handle)
		huser = mgr->user->handle;


	if (!mgr->session) {

		lion_printf(huser, "QUOTE|SID=%u|CODE=1507|MSG=No active session.\r\n",
					mgr->id);
		return;
	}


	if (!args || !*args) {

		lion_printf(huser, "QUOTE|SID=%u|CODE=1509|MSG=Requires at least a command.\r\n",
					mgr->id);
		return;
	}


	args = misc_url_decode(args);


	session_cmdq_newf(mgr->session,
					  flags,
					  HANDLER_QUOTE_REPLY,
					  "%s\r\n",
					  args && *args ? args : "");

	SAFE_FREE(args);

}



//
// Surprisingly enough, CWD is somewhat tricky.
// We get "new_path" from API, so we need to issue CWD to the FTP server
// if this fails, we tell API.
// if it succeeds, we issue PWD.
// if PWD fails (it can, see no-listing FTP directories) we send
// that path is "new_path".
// if PWD succeeds, and we can parse out the reply, we send IT as new path.
//
// So, issue CWD and reply to user. Also issue PWD, but use handler
// _INTERNAL that just fetches the path without sending PWD reply to user.
//
void manager_cwd(manager_t *mgr, char *args, int flags)
{
	lion_t *huser = NULL;
	// Check mgr is valid?
	// Check session is valid?
	// Check session is idle?

	if (mgr && mgr->user && mgr->user->handle)
		huser = mgr->user->handle;


	if (!mgr->session) {

		lion_printf(huser, "CWD|SID=%u|CODE=1507|MSG=No active session.\r\n",
					mgr->id);
		return;
	}


	// Technically checked in command2.c before we are even called.
	if (!args || !*args) {

		lion_printf(huser, "CWD|SID=%u|CODE=1509|MSG=Requires at least a command.\r\n",
					mgr->id);
		return;
	}

	// Check idle
	args = misc_url_decode(args);

	session_cwd(mgr->session, flags, HANDLER_CWD_REPLY, args);

	SAFE_FREE(args);

}


void manager_pwd(manager_t *mgr, int flags)
{
	lion_t *huser = NULL;
	// Check mgr is valid?
	// Check session is valid?
	// Check session is idle?

	if (mgr && mgr->user && mgr->user->handle)
		huser = mgr->user->handle;


	if (!mgr->session) {

		lion_printf(huser, "PWD|SID=%u|CODE=1507|MSG=No active session.\r\n",
					mgr->id);
		return;
	}


	// Check idle

	session_cmdq_new(mgr->session,
					 flags,
					 HANDLER_PWD_REPLY,
					 "PWD\r\n");

}




void manager_size(manager_t *mgr, char *args, int flags)
{
	lion_t *huser = NULL;
	// Check mgr is valid?
	// Check session is valid?
	// Check session is idle?

	if (mgr && mgr->user && mgr->user->handle)
		huser = mgr->user->handle;


	if (!mgr->session) {

		lion_printf(huser, "SIZE|SID=%u|CODE=1507|MSG=No active session.\r\n",
					mgr->id);
		return;
	}


	// Technically checked in command2.c before we are even called.
	if (!args || !*args) {

		lion_printf(huser, "SIZE|SID=%u|CODE=1509|MSG=Requires at least a command.\r\n",
					mgr->id);
		return;
	}

	// Check idle

	session_cmdq_newf(mgr->session,
					  flags,
					 HANDLER_SIZE_REPLY,
					  "SIZE %s\r\n",
					  args);

}








void manager_mkd(manager_t *mgr, char *args, int flags)
{
	lion_t *huser = NULL;
	// Check mgr is valid?
	// Check session is valid?
	// Check session is idle?

	if (mgr && mgr->user && mgr->user->handle)
		huser = mgr->user->handle;


	if (!mgr->session) {

		lion_printf(huser, "MKD|SID=%u|CODE=1507|MSG=No active session.\r\n",
					mgr->id);
		return;
	}

	args = misc_url_decode(args);

	session_cmdq_newf(mgr->session,
					  flags,
					  HANDLER_MKD_REPLY,
					  "MKD %s\r\n",
					  args);

	SAFE_FREE(args);

}



void manager_rmd(manager_t *mgr, char *args, int flags)
{
	lion_t *huser = NULL;
	// Check mgr is valid?
	// Check session is valid?
	// Check session is idle?

	if (mgr && mgr->user && mgr->user->handle)
		huser = mgr->user->handle;


	if (!mgr->session) {

		lion_printf(huser, "RMD|SID=%u|CODE=1507|MSG=No active session.\r\n",
					mgr->id);
		return;
	}

	session_cmdq_newf(mgr->session,
					  flags,
					  HANDLER_RMD_REPLY,
					  "RMD %s\r\n",
					  args);
}




void manager_dele(manager_t *mgr, char *args, int flags)
{
	lion_t *huser = NULL;
	// Check mgr is valid?
	// Check session is valid?
	// Check session is idle?

	if (mgr && mgr->user && mgr->user->handle)
		huser = mgr->user->handle;


	if (!mgr->session) {

		lion_printf(huser, "DELE|SID=%u|CODE=1507|MSG=No active session.\r\n",
					mgr->id);
		return;
	}

	args = misc_url_decode(args);

	session_cmdq_newf(mgr->session,
					  flags,
					  HANDLER_DELE_REPLY,
					  "DELE %s\r\n",
					  args);

	SAFE_FREE(args);

}



void manager_site(manager_t *mgr, char *args, int flags)
{
	lion_t *huser = NULL;
	// Check mgr is valid?
	// Check session is valid?
	// Check session is idle?

	if (mgr && mgr->user && mgr->user->handle)
		huser = mgr->user->handle;


	if (!mgr->session) {

		lion_printf(huser, "SITE|SID=%u|CODE=1507|MSG=No active session.\r\n",
					mgr->id);
		return;
	}

	args = misc_url_decode(args);

	session_cmdq_newf(mgr->session,
					  flags,
					  HANDLER_SITE_REPLY,
					  "SITE %s\r\n",
					  args);

	SAFE_FREE(args);
}



void manager_ren(manager_t *mgr, char *from, char *to, int flags)
{
	lion_t *huser = NULL;
	// Check mgr is valid?
	// Check session is valid?
	// Check session is idle?

	if (mgr && mgr->user && mgr->user->handle)
		huser = mgr->user->handle;


	if (!mgr->session) {

		lion_printf(huser, "REN|SID=%u|CODE=1507|MSG=No active session.\r\n",
					mgr->id);
		return;
	}

	session_cmdq_newf(mgr->session,
					  flags,
					  HANDLER_RNFR_REPLY,
					  "RNFR %s\r\n",
					  from);

	session_cmdq_newf(mgr->session,
					  flags,
					  HANDLER_RNTO_REPLY,
					  "RNTO %s\r\n",
					  to);

}



void manager_mdtm(manager_t *mgr, char *args, int flags)
{
	lion_t *huser = NULL;
	// Check mgr is valid?
	// Check session is valid?
	// Check session is idle?

	if (mgr && mgr->user && mgr->user->handle)
		huser = mgr->user->handle;


	if (!mgr->session) {

		lion_printf(huser, "MDTM|SID=%u|CODE=1507|MSG=No active session.\r\n",
					mgr->id);
		return;
	}

	session_cmdq_newf(mgr->session,
					  flags,
					  HANDLER_MDTM_REPLY,
					  "MDTM %s\r\n",
					  args);

}

















//
// Giving two SIDs, we will create a new queue using them. The SIDs will
// still be "owned" by the user until such a time as they send Go.
//
void *manager_queuenew(manager_t *mgr1, manager_t *mgr2)
{
	lion_t *huser = NULL;
	queue_t *queue = NULL;

	if (mgr1 && mgr1->user && mgr1->user->handle)
		huser = mgr1->user->handle;

	if (!huser && mgr2 && mgr2->user && mgr2->user->handle)
		huser = mgr2->user->handle;



	// Sessions need not be active until we start to process the queue.
	if (mgr1->qid) {
		lion_printf(huser,
					"QUEUENEW|NORTH_SID=%u|SOUTH_SID=%u|CODE=1510|MSG=SID %u already belongs to QID %u\r\n",
					mgr1->id,
					mgr2->id,
					mgr1->id,
					mgr1->qid);
		return NULL;
	}
	if (mgr2->qid) {
		lion_printf(huser,
					"QUEUENEW|NORTH_SID=%u|SOUTH_SID=%u|CODE=1510|MSG=SID %u already belongs to QID %u\r\n",
					mgr1->id,
					mgr2->id,
					mgr2->id,
					mgr2->qid);
		return NULL;
	}

	queue = queue_newnode();

	if (!queue) {
		lion_printf(huser,"QUEUENEW|CODE=1401|NORTH_SID=%u|SOUTH_SID=%u|MSG=Out of memory.\r\n",
					mgr1->id,
					mgr2->id);
		return NULL;
	}


	// queues do not have any pointers back to manager, only reference by
	// id. In queue we would have to search for manager each time, but it
	// simplifies the pointer consistency when we free managers, or users
	// or queues.
	queue->north_sid = mgr1->id;
	queue->south_sid = mgr2->id;

	mgr1->qid = queue->id;
	mgr2->qid = queue->id;

	queue->north_mgr = mgr1;
	queue->south_mgr = mgr2;

	lion_printf(huser,"QUEUENEW|CODE=0|QID=%u|NORTH_SID=%u|SOUTH_SID=%u|MSG=Queue created.\r\n",
				queue->id,
				queue->north_sid,
				queue->south_sid);

    return queue;
}

