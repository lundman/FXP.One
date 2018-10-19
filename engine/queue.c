// $Id: queue.c,v 1.49 2012/08/12 00:17:46 lundman Exp $
//
// Jorgen Lundman 11th November 2004..
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
#include "engine.h"
#include "queue.h"
#include "manager.h"
#include "handler.h"


static unsigned int queue_counter = 1; // 0 means delete me.
THREAD_SAFE static queue_t *queue_head = NULL;




static void queue_process(queue_t *queue);



int queue_init(void)
{
	return 0;
}


void queue_free(void)
{
	queue_t *node, *next;

	for (node = queue_head; node; node = next) {
		next = node->next;

		queue_freenode(node);

	}

	queue_head = NULL;

}








//
// Create a new queue top-node. Which inside has a list of qitems
//
queue_t *queue_newnode(void)
{
	queue_t *result;

	result = malloc(sizeof(*result));

	if (!result) return NULL;

	memset(result, 0, sizeof(*result));

	result->id = queue_counter++;  // fix me, wrap around overlap.

	result->next = queue_head;
	queue_head = result;


	// Just incase
	result->state = QUEUE_IDLE;


	return result;
}




//
// Queue unlick
//
void queue_unlink(queue_t *queue)
{
	queue_t *node, *prev;

	for (node = queue_head, prev = NULL;
		 node; prev = node, node = node->next) {

		if (queue == node) {

			if (!prev)
				queue_head = node->next;
			else
				prev->next = node->next;

			return;

		}

	}

}


//
// Free a queue
//
void queue_freenode(queue_t *node)
{
	qitem_t *qitem, *next;
	//	SAFE_FREE(node);

	SAFE_FREE(node->pasv_line);

	// Setting the id will mark it to be deleted, and unchained later.
	node->id = 0;
	node->state = QUEUE_IDLE;

	// queue_save?

	// Release queue.
	for( qitem = node->items; qitem; qitem = next) {
		next = qitem->next;
		queue_freeitem(qitem);
	}

	// Release error queue.
	for( qitem = node->err_items; qitem; qitem = next) {
		next = qitem->next;
		queue_freeitem(qitem);
	}

	node->num_items  = 0;
	node->num_errors = 0;

	// Disconnect the managers from thinking they belong to a queue
	// However, we only release the Managers if they were ours.
	if (node->north_mgr) {
		node->north_mgr->qid = 0;
		if (!manager_get_user(node->north_mgr))
			manager_release(node->north_mgr);
		node->north_mgr = NULL;
	}

	if (node->south_mgr) {
		node->south_mgr->qid = 0;
		if (!manager_get_user(node->south_mgr))
			manager_release(node->south_mgr);
		node->south_mgr = NULL;
	}

	SAFE_FREE(node->users);
	node->num_users = 0;

	SAFE_FREE(node);
}


// TODO
// Add function that loops the list of queue's and unchains any
// with id = 0, then frees.
//


// why does this take queue?
qitem_t *queue_newitem(queue_t *queue)
{
	qitem_t *node;

	node = (qitem_t *) malloc(sizeof(*node));

	if (!node) return NULL;

	memset(node, 0, sizeof(*node));

	return node;

}



//
// Remove the top item from the queue
// WARNING: NODE IS RELEASED.
void queue_poptop(queue_t *queue)
{
	qitem_t *next;

	if (!queue) return;
	if (queue->num_items <= 0) return;
	if (!queue->items) return;

	debugf("[queue] popping off '%s'\n",
		   queue->items->src.name);

	next = queue->items->next;
	queue_freeitem(queue->items);
	queue->items = next;
	queue->num_items--;

}


void queue_unchain(queue_t *queue, qitem_t *qitem)
{

}


void queue_freeitem(qitem_t *qitem)
{
	int i;

	file_free_static(&qitem->src);
	file_free_static(&qitem->dst);

	for (i = 0; i < qitem->src_num_reasons; i++)
		SAFE_FREE(qitem->src_reasons[i]);

	SAFE_FREE(qitem->src_reasons);
	qitem->src_num_reasons = 0;

	for (i = 0; i < qitem->dst_num_reasons; i++)
		SAFE_FREE(qitem->dst_reasons[i]);

	SAFE_FREE(qitem->dst_reasons);
	qitem->dst_num_reasons = 0;


	SAFE_FREE(qitem);
}


queue_t *queue_findbyqid(unsigned int qid)
{
	queue_t *runner;

	for (runner = queue_head; runner; runner = runner->next) {

		if (runner->id == qid) return runner;

	}

	return NULL;

}




//
// Take a string, convert it to a qitem_t
// If unknown, we return file.
//
qitem_type_t queue_str2type(char *type)
{

	if (!type || !*type)
		return QITEM_TYPE_FILE;

	if (!mystrccmp("STOP", type))
		return QITEM_TYPE_STOP;
	if (!mystrccmp("DIRECTORY", type))
		return QITEM_TYPE_DIRECTORY;

	return QITEM_TYPE_FILE;

}


//
// Convert qitem_type to a string
// returning static strings isn't kosher
//
char *queue_type2str(qitem_type_t type)
{

	switch (type) {
	case QITEM_TYPE_DIRECTORY:
		return "DIRECTORY";
	case QITEM_TYPE_STOP:
		return "STOP";
	default:
		return "FILE";
	}
	return "FILE"; // warnings
}





//
// Insert a new 'qitem' into queue 'queue' at optional position 'at'
// and optionally report outcome to 'handle'.
//
int queue_insert(queue_t *queue, qitem_t *qitem, char *at, lion_t *handle)
{
	int ccase;
	unsigned int insert_pos, pos;
	qitem_t *prev, *node;

	if (!queue || !qitem) {
		if (handle)
			lion_printf(handle,
						"QADD|CODE=1514|MSG=Internal Error: queue OR qitem is NULL\r\n");
		return -1;
	}


	// We need to find where to insert node, based on "at" value.
	ccase = 0;
	insert_pos = 0;

	if (at) {
		if (!mystrccmp("FIRST", at)) {
			ccase = 1;
		} else if (!mystrccmp("LAST", at)) {
			ccase = 2;
		} else if ((insert_pos = atoi(at)) > 0) {
			ccase = 3;
		}
	}


	// If its "first_files" and "default" we promote it to "FIRST".. makes
	// it easier to deal with.


	// Default?

	switch (ccase) {

	case 1: // Insert at FIRST position!
		prev = NULL;
		pos = 0;
		break;

	case 2: // Insert at LAST position!
	for (prev = NULL, node = queue->items, pos = 0;
		 node;
		 prev = node, node = node->next, pos++) {

		//if (!node->next) {
		//	break;
		//}

	}
	break;

	case 3: // Insert at Nth position!
	for (prev = NULL, node = queue->items, pos = 0;
		 node;
		 prev = node, node = node->next, pos++) {

		if (pos == insert_pos) {
			break;
		}
	}

	break;

	default: // Default insert. After files, before directories. Except
        // if directory is MOVEFIRST.
        for (prev = NULL, node = queue->items, pos = 0;
			 node;
			 prev = node, node = node->next, pos++) {

            // Skip movefirst items
            if (node->weight < 0) continue;

			if (node->type == QITEM_TYPE_DIRECTORY) {
				break;
			}

		}

	} // switch


	// Ok, we are to insert node. If prev is set we shall use it to add
	// the node. If prev is NULL, we use the list head.
	if (!prev) {
		qitem->next = queue->items;
		queue->items = qitem;
	} else {
		qitem->next = prev->next;
		prev->next = qitem;
	}

	queue->num_items++;

	// Report back to the user, if so desired
	if (handle) {

		lion_printf(handle,
					"QADD|CODE=0|QID=%u|ITEMS=%u|@=%u|SRC=%s|Msg=Added successfully.|SRCPATH=%s|DSTPATH=%s",
					queue->id, queue->num_items, pos,
					qitem->src_is_north ? "NORTH" : "SOUTH",
					qitem->src.fullpath, qitem->dst.fullpath);

		if (qitem->src.fid)
			lion_printf(handle, "|FID=%u\r\n", qitem->src.fid);
		else
			lion_printf(handle, "\r\n");


	}

	return pos;
}


void queue_list(command2_node_t *user)
{
	queue_t *queue;

	if (!user || !user->handle) return;


	lion_printf(user->handle, "QLIST|BEGIN\r\n");

	for (queue = queue_head;
		 queue;
		 queue=queue->next) {

		lion_printf(user->handle, "QLIST|QID=%u|NORTH=%s|SOUTH=%s|%sITEMS=%u|"
					"STATUS=%s|ERRORS=%u|KB/s=%.2f\r\n",
					queue->id,
					queue->north_mgr->site->name,
					queue->south_mgr->site->name,
					queue_is_subscriber(queue, user) ? "SUBSCRIBED|" : "",
					queue->num_items,
					queue->state == QUEUE_IDLE ? "IDLE" : "PROCESSING",
					queue->num_errors,
					queue->last_cps);

	}

	lion_printf(user->handle, "QLIST|END\r\n");

}




void queue_get(queue_t *queue, lion_t *handle)
{
	qitem_t *node;
	unsigned int pos;
	char *srcpath, *dstpath;

	if (!queue || !handle) return;

	lion_printf(handle, "QGET|QID=%u|BEGIN|ITEMS=%u\r\n",
				queue->id,queue->num_items);

	for (node = queue->items, pos = 0;
		 node;
		 node=node->next, pos++) {

		srcpath = misc_url_encode(node->src.fullpath);
		dstpath = misc_url_encode(node->dst.fullpath);

		lion_printf(handle, "QGET|QID=%u|@=%u|QTYPE=%s|SRC=%s"
					// Source side
					"|SRCPATH=%s|SRCSIZE=%"PRIu64
					"|SRCREST=%"PRIu64
					// Destination
					"|DSTPATH=%s|DSTSIZE=%"PRIu64
					"|DSTREST=%"PRIu64
					"\r\n",

					queue->id, pos, queue_type2str(node->type),
					node->src_is_north ? "NORTH" : "SOUTH",

					srcpath,
					node->src.size,node->src.rest,

					dstpath,
					node->dst.size,node->dst.rest);

		SAFE_FREE(srcpath);
		SAFE_FREE(dstpath);
	}

	lion_printf(handle, "QGET|QID=%u|END\r\n", queue->id);

}




void queue_err(queue_t *queue, lion_t *handle)
{
	qitem_t *node;
	unsigned int pos;
	int i;
	char *srcpath, *dstpath;

	if (!queue || !handle) return;

	lion_printf(handle, "QERR|QID=%u|BEGIN|ITEMS=%u\r\n",
				queue->id, queue->num_errors);

	for (node = queue->err_items, pos = 0;
		 node;
		 node=node->next, pos++) {

		srcpath = misc_url_encode(node->src.fullpath);
		dstpath = misc_url_encode(node->dst.fullpath);

		lion_printf(handle,"QERR|QID=%u|QTYPE=%s|SRC=%s|SRCPATH=%s|DSTPATH=%s",
					queue->id, queue_type2str(node->type),
					node->src_is_north ? "NORTH" : "SOUTH",
					node->src.fullpath,
					node->dst.fullpath);

		// Fix me, url_encode reason!
		for (i = 0; i < node->src_num_reasons; i++) {
			lion_printf(handle, "|SERR_%u=%s",
						i, node->src_reasons[i]);
		}
		for (i = 0; i < node->dst_num_reasons; i++) {
			lion_printf(handle, "|DERR_%u=%s",
						i, node->dst_reasons[i]);
		}

		lion_output(handle, "\r\n", 2);

		SAFE_FREE(srcpath);
		SAFE_FREE(dstpath);

	}

	lion_printf(handle, "QERR|QID=%u|END\r\n", queue->id);

}





void queue_grab(queue_t *queue, command2_node_t *user, int flags)
{

	if (!queue || !user) return;

	if (queue->state != QUEUE_IDLE) {

		if (user) {

			if (flags & 1) {
				queue_subscribe(queue, user);
			}

			lion_printf(user->handle,
						"QGRAB|QID=%u|CODE=1516|%sMSG=Queue is not IDLE.\r\n",
						queue->id,
						(flags & 1) ? "SUBSCRIBE|" : "");

		}

		return;
	}

	// Queue is idle. Let use take sessions if they are still available

	manager_set_user(queue->north_mgr, user);
	manager_set_user(queue->south_mgr, user);


	if (user) {

		lion_printf(user->handle,
					"QGRAB|QID=%u|CODE=0|ITEMS=%u|NORTH_SITEID=%u|SOUTH_SITEID=%u|NORTH_NAME=%s|SOUTH_NAME=%s",
					queue->id, queue->num_items,
					queue->north_mgr->site->id,
					queue->south_mgr->site->id,
					queue->north_mgr->site->name,
					queue->south_mgr->site->name);

		if (queue->north_sid > 0)
			lion_printf(user->handle, "|NORTH_SID=%u",
					queue->north_sid);

		if (queue->south_sid > 0)
			lion_printf(user->handle, "|SOUTH_SID=%u",
						queue->south_sid);

		lion_output(user->handle, "\r\n", 2);
	}


	// Set handler, this will send IDLE/CONNECTED msgs.
	// We do this AFTER QGRAB reply, so API knows which SIDS the CONENCT
	// refer to.
	if (queue->north_mgr->session)
		session_set_handler(queue->north_mgr->session, handler_withuser);
	if (queue->south_mgr->session)
		session_set_handler(queue->south_mgr->session, handler_withuser);

}







void queue_del(queue_t *queue, char *at, lion_t *handle)
{
	qitem_t *node, *prev;
	unsigned int pos;
	unsigned int nat;

	if (!queue || !handle) return;

	nat = atoi(at);

	// If we are active, we can not delete pos 0
	if ((queue->state != QUEUE_IDLE) &&
		(nat == 0)) {
		lion_printf(handle, "QDEL|QID=%u|@=%u|CODE=1404|MSG=QITEM is active/transferring.\r\n",
					queue->id,nat);
		return;
	}


	// Find node.
	for (node = queue->items, pos = 0, prev = NULL;
		 node;
		 prev = node, node=node->next, pos++) {

		if (pos == nat) {

			// Unchain it.
			if (!prev)
				queue->items = node->next;
			else
				prev->next = node->next;

			queue->num_items--;

			queue_freeitem(node);

			lion_printf(handle, "QDEL|QID=%u|CODE=0|@=%u|ITEMS=%u|MSG=OK\r\n",
						queue->id, nat, queue->num_items);
			return;

		}


	}

	lion_printf(handle,
				"QDEL|QID=%u|CODE=1517|MSG=No queue item at position @=%u\r\n",
				queue->id, atoi(at));

}





void queue_move(queue_t *queue, char *from, char *to, lion_t *handle)
{
	qitem_t *node, *prev;
	unsigned int pos;
	int nfrom;

	if (!queue || !handle) return;

	nfrom = atoi(from);

	// If we are not idle, ie, processing a queue...
	// If we are already processing an item, we are not allowed to move
	// first item, or TO first item.
	if (queue->state != QUEUE_IDLE) {

		if (nfrom == 0) {

			lion_printf(handle, "QMOVE|QID=%u|FROM=%u|TO=%s|CODE=1404|MSG=QITEM is active/transferring.\r\n",
						queue->id,nfrom,to);
			return;
		}

		if (atoi(to) == 0) {
			to = "1";
		}
	} // idle?


	// Find node.
	for (node = queue->items, pos = 0, prev = NULL;
		 node;
		 prev = node, node=node->next, pos++) {

		if (pos == nfrom) {

			// Unchain it.
			if (!prev)
				queue->items = node->next;
			else
				prev->next = node->next;

			queue->num_items--; // insert increases it.
			pos = queue_insert(queue, node, to, NULL);

			if (handle)
				lion_printf(handle, "QMOVE|QID=%u|FROM=%u|@=%u|CODE=0|ITEMS=%u|MSG=Moved\r\n",
							queue->id,nfrom,pos,queue->num_items);
			return;

		}


	}

	if (handle)
		lion_printf(handle,
					"QMOVE|QID=%u|CODE=1517|MSG=No queue item at position @=%u\r\n",
					queue->id, atoi(from));

}





//
// Run through the directory_compare function, and compare the directories
// on "both" sides, returning items for GUI to highlight.
//
void queue_compare(queue_t *queue, lion_t *handle)
{
	manager_t *src_mgr, *dst_mgr;

	if (!queue || !handle) return;


	lion_printf(handle,
				"QCOMPARE|QID=%u|BEGIN\r\n",
				queue->id);

	// Assign src to be which-ever side has a dir list.
	if (queue->north_mgr && queue->south_mgr) {

		src_mgr = queue->north_mgr;
		dst_mgr = queue->south_mgr;

		if (src_mgr->session && src_mgr->session->files) {

			lion_printf(handle, "QCOMPARE|QID=%u|NORTH_FID=",
						queue->id);

			queue_directory_think(queue, src_mgr, dst_mgr,
								  dst_mgr->session->pwd, handle);

			lion_printf(handle, "\r\n");

		}

		src_mgr = queue->south_mgr;
		dst_mgr = queue->north_mgr;

		if (src_mgr->session && src_mgr->session->files) {

			lion_printf(handle, "QCOMPARE|QID=%u|SOUTH_FID=",
						queue->id);

			queue_directory_think(queue, src_mgr, dst_mgr,
								  dst_mgr->session->pwd, handle);

			lion_printf(handle, "\r\n");

		}

	} // has mgrs

	lion_printf(handle,
				"QCOMPARE|QID=%u|END|MSG=Completed comparison.\r\n",
				queue->id);

}




















//
// Start queue processing.
// handle is optional, so we can inform user as to if we've started.
//
void queue_go(queue_t *queue, lion_t *handle)
{

	// We remove the users ownership of the Manager
	if (!queue->north_mgr ||
		!queue->south_mgr) {
		if (handle)
			lion_printf(handle, "GO|QID=%u|CODE=1514|MSG=Internal Error: Have not got two mgrs.\r\n",
						queue->id);
		return;
	}


	manager_unlink_user(queue->north_mgr);
	manager_unlink_user(queue->south_mgr);

	queue->state = QUEUE_START;

	if (handle)
		lion_printf(handle, "GO|QID=%u|CODE=0|MSG=Queue processing started.\r\n",
					queue->id);

}




//
// Stop queue processing.
// handle is optional, so we can inform user as to if we've stopped.
//
// <level> : <cause>
//
//    0    : Soft stop. Allow current transfer to finish, then stop.
//    1    : Hard stop. Send ABOR in an attempt to stop transfer now.
//    2    : Drop stop. Just drop sessions if transfer is in progress.
//
void queue_stop(queue_t *queue, lion_t *handle, int level)
{

	// We remove the users ownership of the Manager
	if (!queue->north_mgr ||
		!queue->south_mgr) {
		if (handle)
			lion_printf(handle, "STOP|QID=%u|CODE=1514|MSG=Internal Error: Have not got two mgrs.\r\n",
						queue->id);
		return;
	}

	// Signal to stop.
	queue->stop = 1;

	if (handle)
		lion_printf(handle, "STOP|QID=%u|CODE=0|MSG=Stop initiated, please wait..\r\n",
					queue->id);

	// If we are in a transfer, and hard level is wanted....
	if (queue->state == QUEUE_FILE_PHASE_12_START) {

		switch(level) {

		case 0: // SOFT, do nothing extra
		default:
			break;
		case 1: // HARD, send abort
			session_abor(queue->north_mgr->session);
			session_abor(queue->south_mgr->session);
			break;
		case 2: // DROP
			session_setclose(queue->north_mgr->session, "STOP|DROP issued");
			session_setclose(queue->south_mgr->session, "STOP|DROP issued");
			// Nothing will send the QC here, so we will force it to send
			// queue IDLE.
			queue->state = QUEUE_IDLE;
			queue_process(queue);  // Will send QC|IDLE only.
		}

	}

}









/**
 ** First top level down, we have queue, and its status which is the highest
 ** level. It deals with the synchronisation required with both managers, and
 ** their sessions.
 ** Each Manager also has a state, as well as a handler that is currently
 ** taking the feed back.
 ** However, in the Manager's handler, we only receive the session, and
 ** look up the Manager. We do not, directly, have access to the top level
 ** queue node.
 **
 ** But the queue will often need to wait for both sides to be IDLE.
 **
 ** These are the Phases we go through to transfer a file, in this order.
 **
 ** [ Source ]    &   [Destination]    [Phase]
 **
 **    CWD        &   CWD [ MKD [ CWD ] ]  1 | Go to dir, create if needed.
 **  CCSN/SSCN    &    [CCSN/SSCN]         2 | SSL, send to one of them.
 **   [PROT]      &      [PROT]            3 | SSL, go privacy, or not.
 **    TYPE       &       TYPE             4 | Set Binary, if needed.
 **               &       SIZE             5 | Exists? Size? Resume? Res-Last?
 **   [PRET]      &      [PRET]            6 | If pre-transfer
 **   [PASV]      &      [PASV]            7 | Send pasv to one site
 **   [PORT]      &      [PORT]            8 | Send port to the other.
 **   [REST]      &      [REST]            9 | If resume, send restart point
 **               &       STOR            10 | Attempt stor
 **   [RETR]      &                       11 | Attempt retr
 **    NOOP       &       NOOP            12 | Stop idle out until we are rdy
 **
 **
 ** Incase of failure, at any time, we need to consider:
 ** The CCSN/SSCN state, and to reset?
 ** The PROT level, and to reset?
 ** Did we send REST?  Clear it
 **
 **/



static void queue_process(queue_t *queue)
{
	int i; // Only used for QS and QC.

	// Don't print debug for idle, empty or while transfer
	// is in progress.
	if (queue->state && (queue->state != QUEUE_EMPTY) &&
		(queue->state != QUEUE_FILE_PHASE_12_START))
		debugf("[queue] %u %p state %u\n", queue->id, queue, queue->state);


	// Look for NULL sessions. This means we've failed.
	switch(queue->state) {
		// States we are allowed to be in, and failed.
	case QUEUE_IDLE:
	case QUEUE_START:
	case QUEUE_ISCONNECTED:
	case QUEUE_ITEM_FAILED:
	case QUEUE_ERROR_ITEM:
	case QUEUE_REQUEUE_ITEM:
		//case QUEUE_START_NEXT_ITEM:
	case QUEUE_ACTIVE:
		break;
	default:
		if ((queue->state != QUEUE_IDLE) &&
			queue->north_mgr &&
			queue->south_mgr) {

			if (!queue->north_mgr->session ||
				!queue->south_mgr->session) {

				if (!queue->stop && queue->num_items > 0) {

					debugf("[queue] QID %u failure detected state: %u.\n",
						   queue->id, queue->state);

					queue->state = QUEUE_ITEM_FAILED;

				}
			}
		}
		break;
	}






	switch(queue->state) {

		// We've received "GO" from the API, so let's kick things in.
	case QUEUE_START:
		debugf("[queue] QID %u ordered to start!\n", queue->id);
		queue->state = QUEUE_ISCONNECTED;
		queue->stop = 0;

		if (!queue->items || !queue->num_items)
			queue->state = QUEUE_EMPTY;

#ifndef SLOW_QUEUE
		engine_nodelay = 1;
#endif
		break;

		// First, we do sanity checks to make sure both sites are connected.
	case QUEUE_ISCONNECTED:

		queue->state = QUEUE_START_NEXT_ITEM;

		// Make sure sessions exist
		if (!queue->north_mgr->session) {
			debugf("[queue] north not connected\n");
			queue->north_mgr->session = session_new(handler_queue,
													queue->north_mgr->site);
			queue->north_sid = queue->north_mgr->id;
		} else {
			session_set_handler(queue->north_mgr->session, handler_queue);
		}

		if (!queue->south_mgr->session) {
			debugf("[queue] south not connected\n");
			queue->south_mgr->session = session_new(handler_queue,
													queue->south_mgr->site);
			queue->south_sid = queue->south_mgr->id;
		} else {
			session_set_handler(queue->south_mgr->session, handler_queue);
		}


		break;





		// The we need to wait for both sites to be idle
	case QUEUE_START_NEXT_ITEM:
		if (session_isidle(queue->north_mgr->session) &&
			session_isidle(queue->south_mgr->session)) {
			queue->state = QUEUE_ACTIVE;

#ifndef SLOW_QUEUE
			engine_nodelay = 1;
#endif
		}
		break;






		// Time to consider the Next item in the queue.
		// Work out if it is a DIRECTORY or FILE.
		// TODO, other queue items like STOP, EXPAND?
	case QUEUE_ACTIVE:

        // Lets not get stuck in this state forever...
        queue->state = QUEUE_EMPTY;


		if (queue->num_items <= 0) {
			queue->state = QUEUE_EMPTY;
			break;
		}

		if (queue->stop) {
			debugf("QUEUE_ACTIVE: I see 'stop' is set\n");
			queue->state = QUEUE_IDLE;
			break;
		}


		queue->src_mgr =  queue->items->src_is_north ?
			queue->north_mgr : queue->south_mgr;
		queue->dst_mgr = !queue->items->src_is_north ?
			queue->north_mgr : queue->south_mgr;

		if (queue->items->type == QITEM_TYPE_FILE)
			queue->state = QUEUE_FILE_START;

		if (queue->items->type == QITEM_TYPE_DIRECTORY)
			queue->state = QUEUE_DIRECTORY_START;

		if (queue->items->type == QITEM_TYPE_STOP) {
			queue->stop = 1;
			queue->state = QUEUE_REMOVE_ITEM;
		}


#ifndef SLOW_QUEUE
		engine_nodelay = 1;
#endif
		break;

		// Start processing the file qitem.
	case QUEUE_FILE_START:
		debugf("[queue] file start '%s'\n", queue->items->src.name);

		if (queue->num_users) {
			char *name;

			name = misc_url_encode(queue->items->src.fullpath);
			for (i = 0; i < queue->num_users; i++)
				if (queue->users[i] && queue->users[i]->handle)
					lion_printf(queue->users[i]->handle,
								"QS|QID=%u|START|SRC=%s|SRCPATH=%s\r\n",
								queue->id,
								queue->items->src_is_north ? "NORTH" : "SOUTH",
								name);
			SAFE_FREE(name);
		}

#ifndef SLOW_QUEUE
		engine_nodelay = 1;
#endif

		queue->state = QUEUE_FILE_PHASE_1_START;
		break;





		/**
		 **
		 ** #######   ###   #       #######  #####
		 ** #          #    #       #       #     #
		 ** #          #    #       #       #
		 ** #####      #    #       #####    #####
		 ** #          #    #       #             #
		 ** #          #    #       #       #     #
		 ** #         ###   ####### #######  #####
		 **
		 **/






		/**
		 ** PHASE ONE
		 **
		 ** CWD and MKD
		 **
		 **/

	case QUEUE_FILE_PHASE_1_START:
		// Change to the correct directory.

		queue->state = QUEUE_FILE_PHASE_1_IDLE;

		debugf("[queue] Changing SRC to '%s'\n",
					queue->items->src.dirname);


		session_cwd(queue->src_mgr->session, 0, QUEUE_EVENT_PHASE_1_SRC_CWD,
					queue->items->src.dirname);
		session_cwd(queue->dst_mgr->session, 0, QUEUE_EVENT_PHASE_1_DST_CWD,
					queue->items->dst.dirname);
		break;


	case QUEUE_FILE_PHASE_1_IDLE:
		// Wait for both to be idle so we can check for success or failure
		if (session_isidle(queue->north_mgr->session) &&
			session_isidle(queue->south_mgr->session)) {

			debugf("[queue] QID %u PHASE ONE\n", queue->id);

			if (queue->items->src_failure ||
				queue->items->dst_failure) {
				queue->state = QUEUE_ITEM_FAILED;

#ifndef SLOW_QUEUE
				engine_nodelay = 1;
#endif

				// Now, either SRC dir is bad, or DST dir is bad. We
				// should run through the queue and remove any other items
				// which would have the same issue.
				// TODO

				break;
			}

			queue->state = QUEUE_FILE_PHASE_2_START;
#ifndef SLOW_QUEUE
			engine_nodelay = 1;
#endif
			break;


		}
		break;






		/**
		 ** PHASE TWO
		 **
		 ** CCSN and SSCN challenge!
		 **
		 **/

	case QUEUE_FILE_PHASE_2_START:
		// First we need to determine if it is desirable to attempt
		// secure data.
		// Both sites HAS to be SSLed.
		// secure_data user option needs to be AUTO, or YES.
		if (session_isidle(queue->north_mgr->session) &&
			session_isidle(queue->south_mgr->session)) {

			debugf("[queue] QID %u PHASE TWO\n", queue->id);

			// Both sites SSL?
			if (!(session_feat(queue->north_mgr->session)&FEATURE_SSL) ||
				!(session_feat(queue->south_mgr->session)&FEATURE_SSL)) {

				debugf("[queue] Secure DATA requires both sites to be SSLed-skip\n");

				// No SSL.
				queue->state = QUEUE_FILE_PHASE_2_DISABLE;
#ifndef SLOW_QUEUE
				engine_nodelay = 1;
#endif
				break;

			}

			// We will check for the FXP_TLS variable here, in case
			// user wants secure FXP disable even though we have the
			// capabilities to do it.
			if ((str2yna(sites_find_extra(queue->north_mgr->site, "FXP_TLS"))
				 == YNA_NO) ||
				(str2yna(sites_find_extra(queue->south_mgr->site, "FXP_TLS"))
				 == YNA_NO)) {

				debugf("[queue] Secure DATA Disabled due to FXP_TLS setting\n");

				// No SSL.
				queue->state = QUEUE_FILE_PHASE_2_DISABLE;
#ifndef SLOW_QUEUE
				engine_nodelay = 1;
#endif
				break;
			}

			// Deal with the special case of "local" sites. If one side is
			// local, we do not need to send CCSN/SSCN, just negotiate the
			// "PROT" level, and carry on.
			if (!mystrccmp("<local>", queue->src_mgr->site->host) ||
				!mystrccmp("<local>", queue->dst_mgr->site->host)) {

				debugf("[queue] using local site, enabling SSL\n");

				queue->secure_data = 1;
				queue->state = QUEUE_FILE_PHASE_3_START;

				// Set SSCN/CCSN off, in case it was on previously.
				if ((session_feat(queue->north_mgr->session)&FEATURE_CCSN)) {
					session_ccsn(queue->north_mgr->session, 0,
								 QUEUE_EVENT_PHASE_2_DISABLE,
								 0);
				}
				if ((session_feat(queue->north_mgr->session)&FEATURE_SSCN)) {
					session_sscn(queue->north_mgr->session, 0,
								 QUEUE_EVENT_PHASE_2_DISABLE,
								 0);
				}

				if ((session_feat(queue->south_mgr->session)&FEATURE_CCSN)) {
					session_ccsn(queue->south_mgr->session, 0,
								 QUEUE_EVENT_PHASE_2_DISABLE,
								 0);
				}
				if ((session_feat(queue->south_mgr->session)&FEATURE_SSCN)) {
					session_sscn(queue->south_mgr->session, 0,
								 QUEUE_EVENT_PHASE_2_DISABLE,
								 0);
				}

#ifndef SLOW_QUEUE
				engine_nodelay = 1;
#endif
				break;
			}


			queue->state = QUEUE_FILE_PHASE_2_WAIT;

			queue->secure_data = 0;

			// Does SRC support CCSN/SSCN ?
			if ((session_feat(queue->src_mgr->session)&FEATURE_CCSN)) {
				session_ccsn(queue->src_mgr->session, 0,
							 QUEUE_EVENT_PHASE_2_SRC_SECURE,
							 STATUS_ON_PRIVACY);
				break;
			} else if ((session_feat(queue->src_mgr->session)&FEATURE_SSCN)){
				session_sscn(queue->src_mgr->session, 0,
							 QUEUE_EVENT_PHASE_2_SRC_SECURE,
							 STATUS_ON_PRIVACY);
				break;
			// Does DST support CCSN/SSCN ?
			} else if ((session_feat(queue->dst_mgr->session)&FEATURE_CCSN)){
				session_ccsn(queue->dst_mgr->session, 0,
							 QUEUE_EVENT_PHASE_2_DST_SECURE,
							 STATUS_ON_PRIVACY);
				break;
			} else if ((session_feat(queue->dst_mgr->session)&FEATURE_SSCN)){
				session_sscn(queue->dst_mgr->session, 0,
							 QUEUE_EVENT_PHASE_2_DST_SECURE,
							 STATUS_ON_PRIVACY);
				break;
			}

			debugf("[queue] Secure DATA failed to find ONE site with CCSN/SSCN-skip\n");

			// No such option.
			queue->state = QUEUE_FILE_PHASE_2_DISABLE;
#ifndef SLOW_QUEUE
			engine_nodelay = 1;
#endif


		}

		break;


	case QUEUE_FILE_PHASE_2_WAIT:
		// If IDLE
		if (session_isidle(queue->north_mgr->session) &&
			session_isidle(queue->south_mgr->session)) {

#ifndef SLOW_QUEUE
			engine_nodelay = 1;
#endif

			// Did it fail at all?
			if (queue->items->src_failure ||
				queue->items->dst_failure) {

				queue->state = QUEUE_FILE_PHASE_2_DISABLE;
				break;

			}

			queue->state = QUEUE_FILE_PHASE_3_START;
			break;

		}

		break;



	case QUEUE_FILE_PHASE_2_DISABLE:
		{
			char *value;
			// Disable CCSN/SSCN. Don't forget that the session_ccsn methods
			// will only send the command when it is neccessary.

			queue->secure_data = 0;
			queue->state = QUEUE_FILE_PHASE_3_START;

			if ((session_feat(queue->north_mgr->session)&FEATURE_CCSN)) {
				session_ccsn(queue->north_mgr->session, 0,
							 QUEUE_EVENT_PHASE_2_DISABLE,
							 0);
			}
			if ((session_feat(queue->north_mgr->session)&FEATURE_SSCN)) {
				session_sscn(queue->north_mgr->session, 0,
							 QUEUE_EVENT_PHASE_2_DISABLE,
							 0);
			}

			if ((session_feat(queue->south_mgr->session)&FEATURE_CCSN)) {
				session_ccsn(queue->south_mgr->session, 0,
							 QUEUE_EVENT_PHASE_2_DISABLE,
							 0);
			}
			if ((session_feat(queue->south_mgr->session)&FEATURE_SSCN)) {
				session_sscn(queue->south_mgr->session, 0,
							 QUEUE_EVENT_PHASE_2_DISABLE,
						 0);
			}

			// We KNOW we can not do secure_data here. If that is demanded
			// we will FAIL the file instead of carrying on.

			// Also let optional key/value pair over-ride
			value = sites_find_extra(queue->src_mgr->site, "FXP_TLS");

			if ((queue->src_mgr->site->data_TLS == YNA_YES) ||
				(str2yna(value) == YNA_YES)) {
				queue->state = QUEUE_ITEM_FAILED;
				queue_adderr_src(queue->items,
								 "Secure DATA not possible and source forced to Secure.");

			}

			value = sites_find_extra(queue->dst_mgr->site, "FXP_TLS");

			if ((queue->dst_mgr->site->data_TLS == YNA_YES) ||
				(str2yna(value) == YNA_YES)) {
				queue->state = QUEUE_ITEM_FAILED;
				queue_adderr_dst(queue->items,
								 "Secure DATA not possible and destination forced to Secure.");

			}

#ifndef SLOW_QUEUE
			engine_nodelay = 1;
#endif
		}

		break;






		/**
		 ** PHASE THREE
		 **
		 ** PROT / Privacy level
		 **
		 **/

	case QUEUE_FILE_PHASE_3_START:
		if (session_isidle(queue->north_mgr->session) &&
			session_isidle(queue->south_mgr->session)) {

			debugf("[queue] QID %u PHASE THREE\n", queue->id);

			queue->state = QUEUE_FILE_PHASE_3_WAIT;

			if (queue->secure_data) {

				session_prot(queue->src_mgr->session, 0,
							 QUEUE_EVENT_PHASE_3_SRC_PROT,
							 STATUS_ON_PRIVACY);
				session_prot(queue->dst_mgr->session, 0,
							 QUEUE_EVENT_PHASE_3_DST_PROT,
							 STATUS_ON_PRIVACY);
				break;

			} else {

				session_prot(queue->src_mgr->session, 0,
							 QUEUE_EVENT_PHASE_3_SRC_PROT,
							 0);
				session_prot(queue->dst_mgr->session, 0,
							 QUEUE_EVENT_PHASE_3_DST_PROT,
							 0);
				break;

			}
		}
		break;


	case QUEUE_FILE_PHASE_3_WAIT:
		if (session_isidle(queue->north_mgr->session) &&
			session_isidle(queue->south_mgr->session)) {

			// We don't care that PROT C fails? it really shouldn't?
			if (queue->secure_data) {

				if (queue->items->src_failure ||
					queue->items->dst_failure) {

					queue->state = QUEUE_FILE_PHASE_2_DISABLE;
#ifndef SLOW_QUEUE
					engine_nodelay = 1;
#endif
					break;

				} // failure

			} // data_secure

			queue->state = QUEUE_FILE_PHASE_4_START;
#ifndef SLOW_QUEUE
			engine_nodelay = 1;
#endif

		} // IDLE
		break;





		/**
		 ** PHASE FOUR
		 **
		 ** TYPE
		 **
		 **/

	case QUEUE_FILE_PHASE_4_START:
		if (session_isidle(queue->north_mgr->session) &&
			session_isidle(queue->south_mgr->session)) {

			debugf("[queue] QID %u PHASE FOUR\n", queue->id);

			queue->state = QUEUE_FILE_PHASE_5_START;

			// Do we care if TYPE fails?
			session_type(queue->north_mgr->session, 0,
						 QUEUE_EVENT_PHASE_4_TYPE,
						 STATUS_ON_BINARY);
			session_type(queue->south_mgr->session, 0,
						 QUEUE_EVENT_PHASE_4_TYPE,
						 STATUS_ON_BINARY);

		}
		break;




		/**
		 ** PHASE FIVE
		 **
		 ** SIZE - Determine automatic resume
		 **
		 **/

	case QUEUE_FILE_PHASE_5_START:
		if (session_isidle(queue->north_mgr->session) &&
			session_isidle(queue->south_mgr->session)) {
			debugf("[queue] QID %u PHASE FIVE\n", queue->id);

			// We don't really care about size if we are not resuming
			// however, we get more accurate CPS rates if we know what
			// the size is.

			queue->state = QUEUE_FILE_PHASE_6_START;

			session_size(queue->src_mgr->session, 0,
						 QUEUE_EVENT_PHASE_5_SRC_SIZE,
						 queue->items->src.name);

			// We only care about destination SIZE if we are going to resume.
			if (!(queue->items->flags & QITEM_FLAG_OVERWRITE) &&
				(queue->dst_mgr->site->resume != YNA_NO)) {

				queue->state = QUEUE_FILE_PHASE_5_REST_WAIT;

				session_size(queue->dst_mgr->session, 0,
							 QUEUE_EVENT_PHASE_5_DST_SIZE,
							 queue->items->dst.name);

			}

		}
		break;


	case QUEUE_FILE_PHASE_5_REST_WAIT:
		if (session_isidle(queue->north_mgr->session) &&
			session_isidle(queue->south_mgr->session)) {

			// We now have SIZE from both servers, if possible.
			// We now determine if we should RESUME. And then
			// if we re-queue for RESUME last.

#ifndef SLOW_QUEUE
			engine_nodelay = 1;
#endif

			queue->state = QUEUE_FILE_PHASE_6_START;

			// If user specified rest values, do nothing.
			if (queue->items->src.rest ||
				queue->items->dst.rest) {
				break;
			}



			// If dst HAS a size. and src is BIGGER resume.
			if ((queue->items->dst.size &&  // 0 length dst is not resume
				 (queue->items->src.size > queue->items->dst.size))) {

				// src is bigger, so resume...
				// unless we are supposed to re-queue last.
				if ((queue->dst_mgr->site->resume_last == YNA_YES) &&
					!(queue->items->flags & QITEM_FLAG_REQUEUED)) {

					// re-queue
					queue->items->flags |= QITEM_FLAG_REQUEUED;
					queue->state = QUEUE_REQUEUE_ITEM;

					break;

				} // resume last?

				// Ok resume now.
				queue->items->src.rest = queue->items->dst.size;
				queue->items->dst.rest = queue->items->dst.size;
				break;

			} // resume?


			// If the sizes are now identical, we are done.
			if ((queue->items->dst.size &&
				 (queue->items->src.size == queue->items->dst.size))) {

				queue->state = QUEUE_REMOVE_ITEM;

				for (i = 0; i < queue->num_users; i++)
					if (queue->users[i] && queue->users[i]->handle)
						lion_printf(queue->users[i]->handle,
									"QS|QID=%u|EQUALSIZE\r\n",
									queue->id);

			}


		} // idle

		break;





		/**
		 ** PHASE SIX
		 **
		 ** PRET
		 **
		 **/

	case QUEUE_FILE_PHASE_6_START:
		if (session_isidle(queue->north_mgr->session) &&
			session_isidle(queue->south_mgr->session)) {
			debugf("[queue] QID %u PHASE SIX\n", queue->id);


			queue->state = QUEUE_FILE_PHASE_7_START;

			// if PRET is YES, OR, if PRET is AUTO, and FEATURE_PRET.
			// We don't really check the error code here, maybe FIXME.
			if ((queue->src_mgr->site->pret == YNA_YES) ||
				((queue->src_mgr->site->pret == YNA_AUTO) &&
				 (session_feat(queue->src_mgr->session) & FEATURE_PRET))) {

				session_cmdq_newf(queue->src_mgr->session,
								  0,
								  QUEUE_EVENT_PHASE_6_PRET,
								  "PRET RETR %s\r\n",
								  queue->items->src.fullpath);
			}

			if ((queue->dst_mgr->site->pret == YNA_YES) ||
				((queue->dst_mgr->site->pret == YNA_AUTO) &&
				 (session_feat(queue->dst_mgr->session) & FEATURE_PRET))) {

				session_cmdq_newf(queue->dst_mgr->session,
								  0,
								  QUEUE_EVENT_PHASE_6_PRET,
								  "PRET STOR %s\r\n",
								  queue->items->dst.fullpath);
			}

#ifndef SLOW_QUEUE
			engine_nodelay = 1;
#endif

		}
		break;





		/**
		 ** PHASE SEVEN
		 **
		 ** PASV
		 **
		 **/

	case QUEUE_FILE_PHASE_7_START:
		if (session_isidle(queue->north_mgr->session) &&
			session_isidle(queue->south_mgr->session)) {
			unsigned long remote;
			int dummy;

			debugf("[queue] QID %u PHASE SEVEN\n", queue->id);

			if (queue->stop) {
				queue->state = QUEUE_IDLE;
				break;
			}

			// Determine who shall get PASV. Normally DST will get
			// PASV if there are no preferences. However, SRC might have
			// "only pasv", or, DST might have "only port".

			queue->state = QUEUE_FILE_PHASE_7_WAIT;

			queue->srcpasv = 1;

			if ((queue->src_mgr->site->fxp_passive == YNA_NO) ||
				(queue->dst_mgr->site->fxp_passive == YNA_YES))
				queue->srcpasv = 0;

			if (queue->srcpasv) {

				if (!mystrccmp("<local>", queue->src_mgr->site->host)) {
					// Special PASV for local
					lion_getsockname(queue->dst_mgr->session->handle,
									 &remote, &dummy);
					session_cmdq_newf(queue->src_mgr->session,
									 0,
									 QUEUE_EVENT_PHASE_7_PASV,
									 "PASV %s\r\n",
									 lion_ntoa(remote));

				} else { // Normal src PASV

					session_cmdq_new(queue->src_mgr->session,
									 0,
									 QUEUE_EVENT_PHASE_7_PASV,
									 "PASV\r\n");
				}

			} else { // dst pasv

				if (!mystrccmp("<local>", queue->dst_mgr->site->host)) {
					// Special PASV for local
					lion_getsockname(queue->src_mgr->session->handle,
									 &remote, &dummy);
					session_cmdq_newf(queue->dst_mgr->session,
									 0,
									 QUEUE_EVENT_PHASE_7_PASV,
									 "PASV %s\r\n",
									 lion_ntoa(remote));

				} else { // normal dst PASV

					session_cmdq_new(queue->dst_mgr->session,
									 0,
									 QUEUE_EVENT_PHASE_7_PASV,
									 "PASV\r\n");
				} // is local
			} // is src_pasv
		} // is idle

		break;


	case QUEUE_FILE_PHASE_7_WAIT:
		if (session_isidle(queue->north_mgr->session) &&
			session_isidle(queue->south_mgr->session)) {

#ifndef SLOW_QUEUE
			engine_nodelay = 1;
#endif

			if (queue->items->src_failure ||
				queue->items->dst_failure) {

				// PASV failed.
				queue->state = QUEUE_ITEM_FAILED;
				break;
			}
			queue->state = QUEUE_FILE_PHASE_8_START;
		}
		break;




		/**
		 ** PHASE EIGHT
		 **
		 ** PORT
		 **
		 **/


	case QUEUE_FILE_PHASE_8_START:
		if (session_isidle(queue->north_mgr->session) &&
			session_isidle(queue->south_mgr->session)) {
			debugf("[queue] QID %u PHASE EIGHT\n", queue->id);

			queue->state = QUEUE_FILE_PHASE_8_WAIT;

			if (!queue->srcpasv)
				session_cmdq_newf(queue->src_mgr->session,
								  0,
								  QUEUE_EVENT_PHASE_8_PORT,
								  "PORT %s\r\n",
								  queue->pasv_line);
			else
				session_cmdq_newf(queue->dst_mgr->session,
								  0,
								  QUEUE_EVENT_PHASE_8_PORT,
								  "PORT %s\r\n",
								  queue->pasv_line);

			SAFE_FREE(queue->pasv_line);

		}
		break;



	case QUEUE_FILE_PHASE_8_WAIT:
		if (session_isidle(queue->north_mgr->session) &&
			session_isidle(queue->south_mgr->session)) {

#ifndef SLOW_QUEUE
			engine_nodelay = 1;
#endif

			if (queue->items->src_failure ||
				queue->items->dst_failure) {

				// PORT failed.
				queue->state = QUEUE_ITEM_FAILED;
				break;
			}
			queue->state = QUEUE_FILE_PHASE_9_START;
		}
		break;




		/**
		 ** PHASE NINE
		 **
		 ** REST Challenge for resume.
		 **
		 **/


	case QUEUE_FILE_PHASE_9_START:
		if (session_isidle(queue->north_mgr->session) &&
			session_isidle(queue->south_mgr->session)) {

			debugf("[queue] QID %u PHASE NINE\n", queue->id);
			// Rest values set?


			// Initiate RESUME
			queue->state = QUEUE_FILE_PHASE_9_REST;

			session_rest(queue->src_mgr->session, 0,
						 QUEUE_EVENT_PHASE_9_SRC_REST,
						 queue->items->src.rest);

			session_rest(queue->dst_mgr->session, 0,
						 QUEUE_EVENT_PHASE_9_DST_REST,
						 queue->items->dst.rest);

			break;
		} // size tests to resume?



	case QUEUE_FILE_PHASE_9_REST:
		if (session_isidle(queue->north_mgr->session) &&
			session_isidle(queue->south_mgr->session)) {

			queue->state = QUEUE_FILE_PHASE_10_START;

			// Failed?
			if (queue->items->src_failure ||
				queue->items->dst_failure) {

				session_rest(queue->src_mgr->session, 0,
							 QUEUE_EVENT_PHASE_9_SRC_REST2,
							 0);

				session_rest(queue->dst_mgr->session, 0,
							 QUEUE_EVENT_PHASE_9_DST_REST2,
							 0);
				break;

			}

#ifndef SLOW_QUEUE
			engine_nodelay = 1;
#endif

		}
		break;






		/**
		 ** PHASE TEN
		 **
		 ** STOR
		 **
		 **/

	case QUEUE_FILE_PHASE_10_START:
		if (session_isidle(queue->north_mgr->session) &&
			session_isidle(queue->south_mgr->session)) {
			debugf("[queue] QID %u PHASE TEN\n", queue->id);

			if (queue->stop) {
				queue->state = QUEUE_IDLE;
				break;
			}

			queue->state = QUEUE_FILE_PHASE_10_WAIT;

			queue->items->src_transfer = 0;
			queue->items->dst_transfer = 0;


			// If we want to see X-DUPE lines, send it with LOG.
			session_stor(queue->dst_mgr->session,
						 (queue->dst_mgr->session->status & STATUS_ON_XDUPE) ?
						 SESSION_CMDQ_FLAG_LOG : 0,
						 QUEUE_EVENT_PHASE_10_STOR,
						 QUEUE_EVENT_PHASE_12_DST_226,
						 queue->items->dst.name); // full paths?

			//session_cmdq_newf(queue->dst_mgr->session,
			//			  0,
			//			  QUEUE_EVENT_PHASE_10_STOR,
			//			  "STOR %s\r\n",
			//			  queue->items->dst.name); // full paths?

		}
		break;



	case QUEUE_FILE_PHASE_10_WAIT:
		if (session_isidle(queue->north_mgr->session) &&
			session_isidle(queue->south_mgr->session)) {

#ifndef SLOW_QUEUE
			engine_nodelay = 1;
#endif

			if (queue->items->src_failure ||
				queue->items->dst_failure) {

				queue->state = QUEUE_ITEM_FAILED;
				break;
			}
			queue->state = QUEUE_FILE_PHASE_11_START;
		}
		break;








		/**
		 ** PHASE ELEVEN
		 **
		 ** RETR
		 **
		 **/

	case QUEUE_FILE_PHASE_11_START:
		if (session_isidle(queue->north_mgr->session) &&
			session_isidle(queue->south_mgr->session)) {
			debugf("[queue] QID %u PHASE ELEVEN\n", queue->id);

			queue->state = QUEUE_FILE_PHASE_11_WAIT;

			session_retr(queue->src_mgr->session,
						 0,
						 QUEUE_EVENT_PHASE_11_RETR,
						 QUEUE_EVENT_PHASE_12_SRC_226,
						 queue->items->src.name); // full paths?

			//session_cmdq_newf(queue->src_mgr->session,
			//			  0,
			//			  QUEUE_EVENT_PHASE_11_RETR,
			//			  "RETR %s\r\n",
			//			  queue->items->src.name); // full paths?
		}
		break;


	case QUEUE_FILE_PHASE_11_WAIT:
		if (session_isidle(queue->north_mgr->session) &&
			session_isidle(queue->south_mgr->session)) {
			if (queue->items->src_failure ||
				queue->items->dst_failure) {

				// In an attempt to recover here, we need to send
				// ABOR to the dst since it already got STOR.
				session_cmdq_new(queue->dst_mgr->session,
								 0,
								 QUEUE_EVENT_PHASE_11_ABOR,
								 "ABOR\r\n");

				queue->state = QUEUE_ITEM_FAILED;
				break;
			}


			for (i = 0; i < queue->num_users; i++)
				if (queue->users[i] && queue->users[i]->handle)
					lion_printf(queue->users[i]->handle,
								"QS|QID=%u|XFRACT|SECURE=%s|PASV=%s"
								"|REST=%"PRIu64
								"|SIZE=%"PRIu64
								"\r\n",
								queue->id,
								queue->secure_data ? "YES" : "NO",
								queue->srcpasv ? "SRC" : "DST",
								queue->items->src.rest,
								queue->items->src.size);

#ifndef SLOW_QUEUE
			engine_nodelay = 1;
#endif

			queue->state = QUEUE_FILE_PHASE_12_START;
		}
		break;








		/**
		 ** PHASE TWELVE
		 **
		 ** 226 reply.
		 **
		 **/

	case QUEUE_FILE_PHASE_12_START:

		// FIX ME!!
		// We should confirm we get 226 here! Not some other error message.
		//
		if (session_isidle(queue->north_mgr->session) &&
			session_isidle(queue->south_mgr->session)) {

			if ((queue->items->src_transfer == 2) &&
				(queue->items->dst_transfer == 2)) {

#ifndef SLOW_QUEUE
				engine_nodelay = 1;
#endif

				if (!queue->items->src_failure &&
					!queue->items->dst_failure) {
					float dur, cps;


					debugf("[queue] QID %u TRANSFER COMPLETE.\n", queue->id);

					session_cmdq_new(queue->src_mgr->session,
									 0,
									 QUEUE_EVENT_PHASE_12_NOOP,
									 "NOOP\r\n");

					session_cmdq_new(queue->dst_mgr->session,
									 0,
									 QUEUE_EVENT_PHASE_12_NOOP,
									 "NOOP\r\n");

					gettimeofday(&queue->xfr_end, NULL);

					queue_calc_speed(queue, &dur, &cps);

					for (i = 0; i < queue->num_users; i++)
						if (queue->users[i] && queue->users[i]->handle)
							lion_printf(queue->users[i]->handle,
										"QS|QID=%u|XFREND|SRC=%s|TIME=%.2f"
										"|KB/s=%.2f\r\n",
										queue->id,
										queue->items->src_is_north ? "NORTH" : "SOUTH",
										dur,
										cps / 1024.0);
					queue->last_cps = cps / 1024.0;

					queue->state = QUEUE_REMOVE_ITEM;

					break;

				} // failure

				queue->state = QUEUE_ITEM_FAILED;
				break;

			} // transfer == 2

		} // idle
		break;












		/**
		 **
		 ** ######    ###   ######   #####
		 ** #     #    #    #     # #     #
		 ** #     #    #    #     # #
		 ** #     #    #    ######   #####
		 ** #     #    #    #   #         #
		 ** #     #    #    #    #  #     #
		 ** ######    ###   #     #  #####
		 **
		 **/




		/**
		 ** Queue item is type Directory.
		 **
		 **
		 ** [ Source ]    &   [Destination] [Phase]
		 **
		 **     CWD       &      CWD         1 | Attempt to CWD to dirs.
		 **   DIRLIST     &    [DIRLIST]     2 | Issue dirlist src & maybe dst
		 **   Compare     &     Compare      3 | Compare and queue.
		 **
		 **/


		/**
		 ** PHASE ONE
		 **
		 ** CWD
		 **
		 **/

	case QUEUE_DIRECTORY_START:
		if (session_isidle(queue->north_mgr->session) &&
			session_isidle(queue->south_mgr->session)) {
			debugf("[queue] QID %u Directory PHASE ONE\n", queue->id);

			// Clear errors.
			queue->items->src_failure = 0;
			queue->items->dst_failure = 0;


			queue->state = QUEUE_DIRECTORY_PHASE_1_IDLE;

			if (queue->num_users &&
                queue->items->src.fullpath &&
                queue->items->dst.fullpath) {
				char *src, *dst;

				src = misc_url_encode(queue->items->src.fullpath);
				dst = misc_url_encode(queue->items->dst.fullpath);
				for (i = 0; i < queue->num_users; i++)
					if (queue->users[i] && queue->users[i]->handle)
						lion_printf(queue->users[i]->handle,
									"QS|QID=%u|START"
									"|SRCPATH=%s"
									"|DSTPATH=%s\r\n",
									queue->id,
									src,
									dst);

				SAFE_FREE(src);
				SAFE_FREE(dst);
			}

			// When a qitem is a type directory, we need to
			// Combine dir + name, or, use fullpath.
			debugf("[queue] src CWD to '%s'\n",
				   queue->items->src.fullpath);
			debugf("[queue] dst CWD to '%s'\n",
				   queue->items->dst.fullpath);

			session_cwd(queue->src_mgr->session, 0,
						QUEUE_EVENT_DIRECTORY_PHASE_1_SRC_CWD,
						queue->items->src.fullpath);
			session_cwd(queue->dst_mgr->session, 0,
						QUEUE_EVENT_DIRECTORY_PHASE_1_DST_CWD,
						queue->items->dst.fullpath);
		}
		break;


	case QUEUE_DIRECTORY_PHASE_1_IDLE:

		debugf("src: isidle %d state %d\n"
			   "dst: isidle %d state %d\n",
			   session_isidle(queue->src_mgr->session),
			   queue->src_mgr->session->state,
			   session_isidle(queue->dst_mgr->session),
			   queue->dst_mgr->session->state);

		// Wait for both to be idle so we can check for success or failure
		if (session_isidle(queue->north_mgr->session) &&
			session_isidle(queue->south_mgr->session)) {

			// dst is allowed to fail here, and often will.

			if (queue->items->src_failure) {

				queue->state = QUEUE_ITEM_FAILED;

#ifndef SLOW_QUEUE
				engine_nodelay = 1;
#endif

				// Now, either SRC dir is bad, or DST dir is bad. We
				// should run through the queue and remove any other items
				// which would have the same issue.
				// TODO

				break;
			}

			queue->state = QUEUE_DIRECTORY_PHASE_2_START;
#ifndef SLOW_QUEUE
			engine_nodelay = 1;
#endif
		}

		break;









		/**
		 ** PHASE TWO
		 **
		 ** Dirlist
		 **
		 **/



	case QUEUE_DIRECTORY_PHASE_2_START:
		if (session_isidle(queue->north_mgr->session) &&
			session_isidle(queue->south_mgr->session)) {
			debugf("[queue] QID %u Directory PHASE TWO\n", queue->id);


			queue->state = QUEUE_DIRECTORY_PHASE_2_MKD;


            //
            // Check to see if site has LISTARGS=-lt or similar set
            // if no such variable, use the default, "-lL".
            //


			// Dirlist SRC
			queue->items->src_transfer = 0;
			session_dirlist(queue->src_mgr->session,
                            site_get_listargs(queue->src_mgr->session->site),
							0,
							QUEUE_EVENT_DIRECTORY_PHASE_2_SRC_DIRLIST);

			// If DST CWD was OK, we dirlist there too
			if (!queue->items->dst_failure) {

				queue->items->dst_transfer = 0;
				session_dirlist(queue->dst_mgr->session,
                                site_get_listargs(queue->dst_mgr->session->site),
								0,
								QUEUE_EVENT_DIRECTORY_PHASE_2_DST_DIRLIST);

				break;
			}


			// Pretend we already has DST dirlist (its empty)
			queue->items->dst_transfer = 2;

			// Clear that CWD failure
			queue->items->dst_failure = 0;



		}
		break;



    case QUEUE_DIRECTORY_PHASE_2_MKD:


		if (session_isidle(queue->north_mgr->session) &&
			session_isidle(queue->south_mgr->session)) {

            // Special case for when "skip empty dirs" is NOT set, if
            // SRC transfered, but has 0 entries
            // DST failed (due to no directory)
            // send MKDIR

            // Otherwise, go to next state directly
			queue->state = QUEUE_DIRECTORY_PHASE_2_COMPARE;

            if ((queue->dst_mgr->site->directory_skipempty == YNA_NO) &&
                (queue->items->src_transfer) &&
                (queue->items->dst_transfer == 2) &&
                (queue->src_mgr->session->files_used == 0)) {

                debugf("[queue] detected empty SRC dir, with non-existent DST dir, and skipemptydirs=no\n");

				session_cmdq_newf(queue->dst_mgr->session,
								  0,
								  QUEUE_EVENT_DIRECTORY_PHASE_2_DST_MKD,
								  "MKD %s\r\n",
                                  queue->items->dst.fullpath);

                break;
            }
        }
        break;




	case QUEUE_DIRECTORY_PHASE_2_COMPARE:
		debugf("src: isidle %d state %d\n"
			   "dst: isidle %d state %d\n",
			   session_isidle(queue->src_mgr->session),
			   queue->src_mgr->session->state,
			   session_isidle(queue->dst_mgr->session),
			   queue->dst_mgr->session->state);


		if (session_isidle(queue->north_mgr->session) &&
			session_isidle(queue->south_mgr->session)) {


			if (queue->items->src_transfer &&
				queue->items->dst_transfer) {
				char *dst_fullpath = NULL;

				debugf("[queue] QID %u Directory PHASE TWO\n", queue->id);

				// Compute the new dst_fullpath, we can not rely on
				// ->pwd as the directory may not exist.
				SAFE_COPY(dst_fullpath,queue->items->dst.fullpath);

				for (i = 0; i < queue->num_users; i++)
					if (queue->users[i] && queue->users[i]->handle)
						lion_printf(queue->users[i]->handle,
									"QC|QID=%u|REMOVE|@=0\r\n", queue->id);

				// We have successfully dirlisted this node.
				// We will pop it off now, so that the queue adds go
				// without this directory in mind.
                // We need to remember if it is DMOVEFIRST.
                if (queue_skiplist(queue->dst_mgr->site, &queue->items->src) < 0)
                    queue->dst_mgr->is_dmovefirst = 1;

				queue_poptop(queue);

				// Compare the directories, and queue things we like.
				queue_directory_think(queue, queue->src_mgr, queue->dst_mgr, dst_fullpath, NULL);

				SAFE_FREE(dst_fullpath);

                // Clear DMOVEFIRST
                queue->dst_mgr->is_dmovefirst = 0;

				queue->state = QUEUE_START;

#ifndef SLOW_QUEUE
				engine_nodelay = 1;
#endif

			} // dirlist

		} // idle

		break;
















		/**
		 ** FAILURE
		 **
		 ** Current item failed to transfer.
		 ** This is a permanent failure. We will NOT retry this items
		 ** without user intervention. It is moved into the failed queue.
		 **
		 **/

	case QUEUE_ITEM_FAILED:
		if (queue->items) {
			debugf("[queue] file failed '%s'\n", queue->items->src.name);

			if (queue->items->src_failure) {
				debugf("[queue] source failed: %d:%s\n",
					   queue->items->src_failure,
					   (queue->items->src_num_reasons > 0) ?
					   queue->items->src_reasons[ queue->items->src_num_reasons - 1] :
					   "Unknown Reason");

				for (i = 0; i < queue->num_users; i++)
					if (queue->users[i] && queue->users[i]->handle)
						lion_printf(queue->users[i]->handle,
									"QS|QID=%u|FAILED|SERR=%s|COUNT=%u\r\n",
									queue->id,
									(queue->items->src_num_reasons > 0) ?
									queue->items->src_reasons[ queue->items->src_num_reasons - 1] :
									"Unknown Reason",
									queue->items->soft_errors);

			}

			if (queue->items->dst_failure) {
				debugf("[queue] destination failed: %d:%s\n",
					   queue->items->dst_failure,
					   (queue->items->dst_num_reasons > 0) ?
					   queue->items->dst_reasons[ queue->items->dst_num_reasons - 1] :
					   "Unknown Reason");

				for (i = 0; i < queue->num_users; i++)
					if (queue->users[i] && queue->users[i]->handle)
						lion_printf(queue->users[i]->handle,
									"QS|QID=%u|FAILED|DERR=%s|COUNT=%u\r\n",
									queue->id,
									(queue->items->dst_num_reasons > 0) ?
									queue->items->dst_reasons[ queue->items->dst_num_reasons - 1] :
									"Unknown Reason",
									queue->items->soft_errors);

			}

			// Increase soft_errors
			queue->items->soft_errors++;


			if (queue->items->soft_errors >= 2) {  // HARDCODED!! FIXME

				// Move item to error queue
				queue->state = QUEUE_ERROR_ITEM;

			} else {

				queue->state = QUEUE_REQUEUE_ITEM;

			}

#ifndef SLOW_QUEUE
			engine_nodelay = 1;
#endif
			break;


		}

		queue->state = QUEUE_START;

#ifndef SLOW_QUEUE
		engine_nodelay = 1;
#endif

		break;





		/**
		 ** FAILURE RE-QUEUE
		 **
		 ** Re-queue item to "last" for retry.
		 ** This is a retryable failure. We will try this item again.
		 **
		 **/

	case QUEUE_REQUEUE_ITEM:
		{
			qitem_t *qitem;
			int npos;

			qitem = queue->items;

			// Queue modifcation here is ugly! Wrapper function.
			queue->items = qitem->next;

			// Queue_insert does num_items++
			queue->num_items--;

			npos = queue_insert(queue, qitem, NULL, NULL);

			for (i = 0; i < queue->num_users; i++)
				if (queue->users[i] && queue->users[i]->handle)
					lion_printf(queue->users[i]->handle,
								"QC|QID=%u|MOVE|FROM=0|TO=%u\r\n",
								queue->id, npos);

			debugf("[queue] Re-queued item from 0 to %u\n", npos);

			//poptop FREEs item
			//queue->state = QUEUE_REMOVE_ITEM;

			queue->state = QUEUE_START;


			queue->src_mgr = NULL;
			queue->dst_mgr = NULL;

			if (queue->stop) {
				queue->state = QUEUE_IDLE;
				break;
			}

#ifndef SLOW_QUEUE
			engine_nodelay = 1;
#endif

		}
		break;





		/**
		 ** RE-QUEUE ITEM INTO ERROR QUEUE
		 **
		 **
		 **/

	case QUEUE_ERROR_ITEM:
		{
			qitem_t *qitem;

			qitem = queue->items;

			// Queue modifcation here is ugly! Wrapper function.
			queue->items = qitem->next;

			// Direct linked-list operations.. wrappers! FIXME
			qitem->next = queue->err_items;
			queue->err_items = qitem;
			//queue_insert(queue, qitem, NULL, NULL);
			queue->num_items--;
			queue->num_errors++;

			queue->state = QUEUE_START;

			for (i = 0; i < queue->num_users; i++)
				if (queue->users[i] && queue->users[i]->handle)
					lion_printf(queue->users[i]->handle,
								"QC|QID=%u|REMOVE|@=0\r\n", queue->id);

			if (queue->stop) {
				queue->state = QUEUE_IDLE;
				break;
			}

			debugf("Cleared REMOVE\n");

			queue->src_mgr = NULL;
			queue->dst_mgr = NULL;

#ifndef SLOW_QUEUE
			engine_nodelay = 1;
#endif

		}
		break;









		/**
		 ** QUEUE ITEM DONE
		 **
		 ** Remove the top item from the queue, process next.
		 **
		 **/
	case QUEUE_REMOVE_ITEM:
		for (i = 0; i < queue->num_users; i++)
			if (queue->users[i] && queue->users[i]->handle)
				lion_printf(queue->users[i]->handle,
						"QC|QID=%u|REMOVE|@=0\r\n", queue->id);

		queue_poptop(queue);
		queue->state = QUEUE_START;

		debugf("Cleared REMOVE 2\n");

		queue->src_mgr = NULL;
		queue->dst_mgr = NULL;

		if (queue->stop) {
			queue->state = QUEUE_IDLE;
			break;
		}

#ifndef SLOW_QUEUE
		engine_nodelay = 1;
#endif

		break;

	case QUEUE_EMPTY:
		for (i = 0; i < queue->num_users; i++)
			if (queue->users[i] && queue->users[i]->handle)
				lion_printf(queue->users[i]->handle,
							"QC|QID=%u|EMPTY|ERRORS=%u\r\n",
							queue->id, queue->num_errors);

		debugf("Sending 'QUIT' to any connected FTP servers\n");
		if (queue->north_mgr)
			session_cmdq_new(queue->north_mgr->session,
				0,
				QUEUE_EVENT_PHASE_12_NOOP,
				"QUIT\r\n");
		if (queue->south_mgr)
			session_cmdq_new(queue->south_mgr->session,
				0,
				QUEUE_EVENT_PHASE_12_NOOP,
				"QUIT\r\n");

		queue->state = QUEUE_IDLE;
		break;

	case QUEUE_IDLE:
		// If we were told to stop, send msg
		if (queue->stop) {
			queue->stop = 0;

			debugf("IDLE: with stop, sending IDLE\n");

			for (i = 0; i < queue->num_users; i++)
				if (queue->users[i] && queue->users[i]->handle)
					lion_printf(queue->users[i]->handle,
								"QC|QID=%u|IDLE|MSG=STOP completed.\r\n",
								queue->id);
		}
		break;


	default:
		break;
	}

}






//
// This function is called periodically, so we can process our queues.
//
void queue_run(void)
{
	queue_t *runner;

	// Look for queues that are processing.
	for (runner = queue_head; runner; runner = runner->next) {

		queue_process(runner);

	}

}















//
// The TOP qitem failed, either soft or hard. Process here.
//
void queue_item_failed(queue_t *queue)
{



}



void queue_adderr_src(qitem_t *qitem, char *line)
{
	char **tmp;

	qitem->src_failure++;


	if (line && *line) {

		debugf("[queue] src err %d '%s'\n",
			   qitem->src_num_reasons, line);

		tmp = (char **)realloc(qitem->src_reasons,
							   (qitem->src_num_reasons + 1) * sizeof(char *));

		if (!tmp) return; // No memory to store this reason.

		qitem->src_reasons = tmp;

		qitem->src_reasons[ qitem->src_num_reasons ] = strdup(line);

		qitem->src_num_reasons++;

	}

}


void queue_adderr_dst(qitem_t *qitem, char *line)
{
	char **tmp;

	qitem->dst_failure++;


	if (line && *line) {

		debugf("[queue] dst err %d '%s'\n",
			   qitem->dst_num_reasons, line);

		tmp = (char **)realloc(qitem->dst_reasons,
							   (qitem->dst_num_reasons + 1) * sizeof(char *));

		if (!tmp) return; // No memory to store this reason.

		qitem->dst_reasons = tmp;

		qitem->dst_reasons[ qitem->dst_num_reasons ] = strdup(line);

		qitem->dst_num_reasons++;

	}

}







void queue_directory_think(queue_t *queue,
						   manager_t *src_mgr, manager_t *dst_mgr,
						   char *dst_fullpath, lion_t *handle)
{
	file_t **files, *dst_file;
	int i, num_files;
	qitem_t *qitem;
	int src_is_north, npos;
	int weight;
	int num_sent = 0;

	debugf("[queue] directory think\n");


	// Is this naughty, assuming the file cache exists?
	num_files = src_mgr->session->files_used;
	files = src_mgr->session->files;

	src_is_north = (src_mgr == queue->north_mgr);



	for (i = 0 ; i < num_files; i++) {
		debugf("[queue] thinking about '%s' (%"PRIu64")...",
			   files[i]->name,
			   files[i]->size);

		// Skip list? (passlist, movefirst...)
		weight = queue_skiplist(dst_mgr->site, files[i]);

        // We also need to test the "parent" directory, since if it was
        // dmovefirst, this file should also move first.
        if (!weight && dst_mgr->is_dmovefirst)
            weight = -1;

		debugf("weight %d ", weight);

		if (weight > 0) {
			debugf(" skip.\n");
			continue;
		}

		// Does it exist on dst? (with fuzz?)
		dst_file = session_getname(dst_mgr->session, files[i]->name);

		// If it is bigger, or the same size, we ignore it
		if (dst_file && (dst_file->directory != YNA_YES) &&
			(dst_file->size >= files[i]->size)) {
			printf("identical, skip.\n");
			continue;
		}

		// If it's a directory, we assume it exists and is complete.
		// This is ONLY in compare mode. During transfer, naturally
		// we should decend.
		if (handle && dst_file && (dst_file->directory == YNA_YES)) {
			printf("identical, skip.\n");
			continue;
		}

		// Are we in QCOMPARE and just reporting?
		if (handle) {
			if (!num_sent++)
				lion_printf(handle,"%u", files[i]->fid);
			else
				lion_printf(handle,",%u", files[i]->fid);
			if (num_sent > 100) {
				lion_printf(handle,"\r\nQCOMPARE|QID=%u|%s=",
							queue->id,
							src_is_north ? "NORTH_FID" : "SOUTH_FID");
				num_sent = 0;
			}
			debugf("\n");
			continue;
		}


		// Create qitem
		qitem = queue_newitem(queue);
		if (!qitem) continue;

		if (files[i]->directory == YNA_YES)
			qitem->type = QITEM_TYPE_DIRECTORY;
		else
			qitem->type = QITEM_TYPE_FILE;

        qitem->weight = weight; // movefirst etc, save us looking it up

		file_dupe(&qitem->src, files[i]);

		// If there was a match on remote, copy it over.
		// This ensures we keep the case the same for existing files.
		// We don't bother with filesize, we issue SIZE command automatically.
		if (dst_file && dst_file->name)
			SAFE_COPY(qitem->dst.name, dst_file->name);

		file_makedest(&qitem->dst, &qitem->src,
					  dst_fullpath);

		// Assign the source
		qitem->src_is_north = src_is_north;


        // Directories with weight == -1, will be queued after files with -1,
        // but before everything else.
        if ((qitem->type == QITEM_TYPE_DIRECTORY) &&
            (qitem->weight == -1)) {
            qitem_t *q;
            char pos[16];
            // Skip all files with weight==-1
            for(npos = 0, q = queue->items;
                q;
                npos++, q = q->next) {

                if ((q->type == QITEM_TYPE_FILE) &&
                    (q->weight == -1)) continue;
                if (q->type == QITEM_TYPE_STOP) continue;
                break;
            } // for

            // If this was C++, we could have a type int too.
            snprintf(pos, sizeof(pos), "%u", npos);
            npos = queue_insert(queue, qitem,
                                pos,
                                NULL);

        } else {

            // Queue it, if weight = -1, make it first, otherwise, default.
            // However, if the first entry is a STOP, we shall respect that.
            if (queue->items &&   // there is a first item
                (weight == -1) && // and we are adding movefirst hit
                (queue->items->type == QITEM_TYPE_STOP)) // but first item is stop
                npos = queue_insert(queue, qitem,
                                    "1",                 // queue after STOP
                                    NULL);
            else
                npos = queue_insert(queue, qitem,
                                    weight == 0 ? NULL : "FIRST",
                                    NULL);

        }

		debugf("(qpos %d) ", npos);

		if (queue->num_users) {
			char *src, *dst;
			int j;

			src = misc_url_encode(qitem->src.fullpath);
			dst = misc_url_encode(qitem->dst.fullpath);

			// We will tag these (possibly many) QC|INSERT as "EXPANDING"
			// and follow with
			for (j = 0; j < queue->num_users; j++)
				if (queue->users[j] && queue->users[j]->handle)
					lion_printf(queue->users[j]->handle,
						"QC|QID=%u|INSERT|@=%u|SRC=%s|SRCPATH=%s|DSTPATH=%s"
						"|QTYPE=%s"
						"|DATE=%lu"
						"|SRCSIZE=%"PRIu64
						"|EXPANDING\r\n",
						queue->id,
						npos,
						qitem->src_is_north ? "NORTH" : "SOUTH",
						src,
						dst,
						queue_type2str(qitem->type),
						(unsigned long)qitem->src.date,
						qitem->src.size);
			SAFE_FREE(src);
			SAFE_FREE(dst);
		}

		debugf("queued.\n");
	}

	// We may have sent lots of EXPANDING lines, so send a REFRESH
	// suggestion to the clients.
	if (queue->num_users && num_files >= 1) {
		int j;
		for (j = 0; j < queue->num_users; j++)
			if (queue->users[j] && queue->users[j]->handle)
				lion_printf(queue->users[j]->handle,
					"QS|REFRESH\r\n");
	}
}




//
// Process everything with a node wrt to skiplist, passlist, skipempty etc.
//
// *) pass "skipempty"
// *) pass "passlist"
// *) pass "skiplist"
//
//  0 - item passed
// !0 - skip item
int queue_skiplist(sites_t *site, file_t *file)
{

	if (file->directory == YNA_YES) {

		// Directory

		//
		if (site->directory_passlist &&
			(!file_listmatch(site->directory_passlist, file->name))) {
			return 2;
		}

		if (site->directory_skiplist &&
			(file_listmatch(site->directory_skiplist, file->name))) {
			return 3;
		}

		// Is it a move first item?
		if (site->directory_movefirst &&
			(file_listmatch(site->directory_movefirst, file->name))) {
			return -1;
		}

		// Pass the directory.
		return 0;

	}






	// File
	if (!file->size &&
		(site->file_skipempty != YNA_NO)) {
		return 1;
	}

	//
	if (site->file_passlist &&
		(!file_listmatch(site->file_passlist, file->name))) {
		return 2;
	}

	if (site->file_skiplist &&
		(file_listmatch(site->file_skiplist, file->name))) {
		return 3;
	}

	// Is it a move first item?
	if (site->file_movefirst &&
		(file_listmatch(site->file_movefirst, file->name))) {
		return -1;
	}

	// Pass the file.
	return 0;

}





int queue_is_subscriber(queue_t *queue, command2_node_t *user)
{
	int i;

	for (i = 0; i < queue->num_users; i++)
		if (queue->users[i] == user) return 1;

	return 0;
}

//
// Add user to list of subscribers.
//
void queue_subscribe(queue_t *queue, command2_node_t *user)
{
	command2_node_t **tmp;
	int i;

	// Don't add user if they are already subscribed
	if (!queue_is_subscriber(queue, user)) {

		// Attempt to find an empty bay first.
		for (i = 0; i < queue->num_users; i++)
			if (!queue->users[i]) {
				queue->users[i] = user;
				return;
			}

		// Allocate a new bay
		tmp = realloc(queue->users,
					  sizeof(command2_node_t *) * (queue->num_users + 1));
		if (!tmp) return;

		queue->users = tmp;

		queue->users[ queue->num_users ] = user;
		queue->num_users++;

	}

}




void queue_unsubscribe(queue_t *queue, command2_node_t *user)
{
	int i;

	for (i = 0; i < queue->num_users; i++)
		if (queue->users[i] == user)
			queue->users[i] = NULL;

}


void queue_unsubscribe_all(command2_node_t *user)
{
	queue_t *runner;

	for (runner = queue_head; runner; runner=runner->next) {
			queue_unsubscribe(runner, user);
	}
}


void queue_calc_speed(queue_t *queue, float *dur, float *cps)
{

	long timesec, timeusec;
	long hseconds;
	float speedthisfile;

	*dur = 0.0;
	*cps = 0.0;

	// How much time has lapsed?
	timesec = queue->xfr_end.tv_sec - queue->xfr_start.tv_sec;

	// If endtimeusec is less than starttime... what then?
	if (queue->xfr_end.tv_usec < queue->xfr_start.tv_usec) {
		timesec--;
		timeusec = (queue->xfr_end.tv_usec + 1000000) - queue->xfr_start.tv_usec;
	} else {
		timeusec = queue->xfr_end.tv_usec - queue->xfr_start.tv_usec;
	}

	// Go to the nearest 10th of a second
	hseconds = timesec * 100;
	hseconds += (timeusec / 10000);

	if (hseconds == 0)
		speedthisfile = 0;
	else {
		// Speed for this file in tenths of seconds, then up for per second.
		speedthisfile = (float)(queue->items->src.size / (float)hseconds);
		speedthisfile *= 100.0;
	}

	*cps = speedthisfile;
	*dur = hseconds / 100.0;

}


int queue_isidle(queue_t *queue)
{

	return (queue->state == QUEUE_IDLE);

}



int queue_getqitem_byname(queue_t *queue, char *srcname, char *dstname)
{
	qitem_t *qitem;
	int pos = 0;

	for( qitem = queue->items, pos = 0;
		 qitem;
		 qitem = qitem->next, pos++) {

		if (srcname && !mystrccmp(srcname, qitem->src.name))
			return pos;
		if (dstname && !mystrccmp(dstname, qitem->dst.name))
			return pos;
	}

	return -1;
}



void queue_xdupe_item(queue_t *queue, char *name)
{
	int pos;
	char *decode;
	char buffer[20];

	debugf("[queue] XDUPE '%s'\n", name);

	decode = misc_url_decode(name);

	// Attempt to locate named file. If found, re-queue it last.
	pos = queue_getqitem_byname(queue, NULL, decode);
	SAFE_FREE(decode);

	if (pos == -1)
		return;

	// It COULD be the item is actually the item WE just finished
	if (!pos) {
		debugf("[queue] XDUPE is for our item, ignoring.\n");
		return;
	}

	// Re-queue it.
	// Not happy that I have to print the position into a string to
	// pass. We should use enums internally.

	debugf("[queue] re-queuing\n");
	snprintf(buffer, sizeof(buffer), "%u", (unsigned int)pos);
	queue_move(queue, buffer, "LAST", NULL);


}


qitem_t *queue_dupeitem(qitem_t *src)
{
    qitem_t *qitem;

    qitem = queue_newitem(NULL);
    if (!qitem) return NULL;

    qitem->type = src->type;
    file_dupe(&qitem->src, &src->src);
    file_dupe(&qitem->dst, &src->dst);
    qitem->src_is_north = src->src_is_north;
    qitem->weight = src->weight;
    qitem->flags = src->flags;
    // We don't copy over the errors, but let new queue has own error counters
    //qitem->src_failure = src->failure;
    //qitem->dst_failure = src->dst_failure;
    //qitem->soft_errors = src->soft_errors;

    return qitem;
}




//
// Make a copy of an existing queue.
//
queue_t *queue_clone(queue_t *src, command2_node_t *user)
{
    queue_t *newq = NULL;
    manager_t *north_mgr, *south_mgr;
    qitem_t *node, *qitem;

    // First create two new managers for sessions
    north_mgr = manager_new(user, src->north_mgr->site, 0);
    if (!north_mgr) {
        debugf("[queue] failed to create north manager\n");
        return NULL;
    }
    south_mgr = manager_new(user, src->south_mgr->site, 0);
    if (!south_mgr) {
        debugf("[queue] failed to create south manager\n");
        return NULL;
    }

    // Then create a new queue controlling both
    newq = manager_queuenew(north_mgr, south_mgr);

    if (!newq) {
        debugf("[queue] failed to create new queue\n");
        return NULL;
    }

    // Iterate the queue and dupe
	for (node = src->items;
		 node;
		 node=node->next) {

        qitem = queue_dupeitem(node);
        if (qitem) {
            queue_insert(newq, qitem, "LAST", NULL);
        }
    }

    debugf("[queue] clone completed: %u.\n", newq->id);
    return newq;
}



//
// If we need to create .../foo/bar/dir/ we return the right
// part based on queue->mkd_depth. Ie;
// depth = 3, return "foo"
// depth = 2, return "bar"
// depth = 1, return "dir"

char *get_mkdname_by_depth(queue_t *queue, int decrease)
{
	int i;
	static char *store = NULL;
	char *r;

	// After MKD, we try to CWD. Just return the same path.
	if (!decrease) {
		if (!store)
			return queue->items->dst.dirname;
		else
			return store;
	}

	if (queue->mkd_depth <= 1) {
		queue->mkd_depth = 0;
		SAFE_FREE(store);
		debugf("[XXXX] returning '%s' depth cleared\n",
			   queue->items->dst.dirname);
		return queue->items->dst.dirname;
	}

	if (!store) {
		// First call? allocate memory, copy string
		SAFE_COPY(store, queue->items->dst.dirname);
	} else {
		// others, just re-copy it.
		strcpy(store, queue->items->dst.dirname);
	}

	// Make sure it doesn't end with "/"
	misc_stripslash(store);

	for (i = 1; i < queue->mkd_depth; i++) {

		if ((r = strrchr(store, '/')))
			*r = (char) 0;

	}

	queue->mkd_depth--;

	debugf("[XXXX] returning '%s' depth %d\n",
		   store, queue->mkd_depth);

	return store;

}

