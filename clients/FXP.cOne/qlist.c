#include <stdio.h>
#include <cdk/cdk.h>

#include "lion.h"
#include "misc.h"

#include "traverse.h"
#include "main.h"
#include "parser.h"
#include "qlist.h"
#include "qmgr.h"
#include "engine.h"
#include "display.h"



static CDKSCROLL           *queue_list  = NULL;
static CDKITEMLIST         *refresh_int = NULL;
static CDKLABEL            *info        = NULL;
static CDKSCROLL           *msgs_viewer = NULL;


static int begx, begy, height, width;

static int num_queues = 0;
static int qlist_counter = 0;

static qlist_t *qlist_head = NULL;



#define QLIST_GOTO_DISPLAY  1
#define QLIST_GOTO_QUEUEMGR 2


static int qlist_leave = 0;

static int current_refresh_interval = 0;  // 0 = never, 1=20s, ...
static int current_refresh_seconds  = 0;  // Auto assigned from above var..



#define GET_NODE it = getCDKScrollCurrent(queue_list); \
		          for (i = 0, node = qlist_head; node && (i < it); node = node->next, i++)	/* empty */ ;




static void qlist_qgrab_sub(int sub, char *msg)
{
	qlist_t *node;
	int i, it;

	switch(sub) {

	case 0: // failed
		botbar_printf("QGRAB failed: %s", msg ? msg : "unknown error");
		break;

	case 1: // qgrab OK
		botbar_print("QGRAB Successful");
		qlist_counter = 0;
		qlist_leave = QLIST_GOTO_DISPLAY;
		break;

	case 2: // subscribe OK
		botbar_print("Subscribe Successful");

		// Assign the site names here.
		GET_NODE;
		if (node) {

		}

		qlist_counter = 0;
		qlist_leave = QLIST_GOTO_QUEUEMGR;
		break;

	}

}

















		
static int queues_callback(EObjectType cdktype, void *object, 
						   void *clientData, chtype input)
{
	//CDKSCROLL *lscroll = (CDKSCROLL *) object;
	MEVENT event;
	int i, it;
	qlist_t *node;

	if (input == KEY_MOUSE) 
		if (getmouse(&event) == OK) {
			check_all_mouse(&event);
		} // get event

	switch(input) {
	case KEY_ENTER:
		
		// Attempt to QGRAB this site.
		GET_NODE;

		if (node)
			engine_qgrab(node->qid, 1, qlist_qgrab_sub);
		break;


	case 'c':
	case 'C':
		GET_NODE;

		if (node) {
			engine_queuefree(node->qid);
			qmgr_printf("Releasing queue (%u)\n", node->qid);
		}
		break;

	case 's':
	case 'S':
		GET_NODE;

		if (node) {
			engine_subscribe(node->qid);
		}
		break;

	}

	return 1;
}
#undef GET_NODE



static void qlist_clear(void)
{
	qlist_t *node, *next;
	char *empty[] = { "Getting queues... " };

	for (node = qlist_head; node; node = next) {
		next = node->next;
		SAFE_FREE(node->left_name);
		SAFE_FREE(node->right_name);
		SAFE_FREE(node);
	}

	qlist_head = NULL;
	num_queues = 0;


	// Clear the display
	setCDKScrollItems(queue_list, 
					  empty,
					  1,
					  TRUE);

}





static int refresh_callback(EObjectType cdktype, void *object, 
							void *clientData, chtype input)
{
	CDKITEMLIST *path = (CDKITEMLIST *)object;
	int current;

	current = getCDKItemlistCurrentItem(path);

	if (current != current_refresh_interval) {
		current_refresh_interval = current;
		switch(current) {
		case 0:  // Never
			current_refresh_seconds = 0;
			break;
		case 1:  // 20s
			current_refresh_seconds = 20;
			break;
		case 2:  // 40s
			current_refresh_seconds = 40;
			break;
		case 4:  // 1m
			current_refresh_seconds = 60;
			break;
		case 5:  // 2m
			current_refresh_seconds = 120;
			break;
		case 6:  // 5m
			current_refresh_seconds = 300;
			break;
		case 7:  // 10m
			current_refresh_seconds = 600;
			break;
		}
	}


	return 1;
}


static int refresh_precallback(EObjectType cdktype, void *object, 
							   void *clientData, chtype input)
{

	if (input == KEY_ENTER) {
		// One shot refresh
		current_refresh_seconds = -1;
	}

	return 1;

}




static void qlist_get_queues_sub(char **keys, char **values, int items)
{
	char *qid, *north, *south, *bits, *status, *errors, *cps, *subbed;
	char buffer[1024];
	qlist_t *qnew;

	if (!items||!values) {

		// Delete the first line in queue "Getting queues..". We do this here
		// as it does a full werase() under certain situations, which is ugly.
		// It'll be the last item.
		setCDKScrollPosition(queue_list, 9999);
		deleteCDKScrollItem(queue_list, 
							getCDKScrollCurrent(queue_list));
		setCDKScrollPosition(queue_list, 0);

		// We're done
		drawCDKScroll(queue_list, TRUE);

		botbar_printf("Retreived %u queue%s",
					  num_queues,
					  num_queues == 1 ? "" : "s");

		return;

	}

	// "QLIST|QID=1|NORTH=ftp|SOUTH=rog|ITEMS=0|STATUS=IDLE|ERRORS=0|KB/s=0.00"
	
	qid   = parser_findkey(keys, values, items, "QID");
	north = parser_findkey(keys, values, items, "NORTH");
	south = parser_findkey(keys, values, items, "SOUTH");
	bits  = parser_findkey(keys, values, items, "ITEMS");
	status= parser_findkey(keys, values, items, "STATUS");
	errors= parser_findkey(keys, values, items, "ERRORS");
	cps   = parser_findkey(keys, values, items, "KB/s");
	subbed= parser_findkey(keys, values, items, "SUBSCRIBED");

	if (!qid || !north || !south || !bits || !status || !errors || !cps)
		return;

	snprintf(buffer, sizeof(buffer), "%s</8>%10s <=> %-10s<!8> (%s/%s) %s at %s KB/s",
			 subbed ? "*" : " ",
			 north,
			 south,
			 bits,
			 errors,
			 status,
			 cps);


	// Create a mini linked-list
	qnew = (qlist_t *)calloc(sizeof(qlist_t), 1);
	if (!qnew) return;

	qnew->next = qlist_head;
	qlist_head = qnew;
	qnew->qid = strtoul(qid, NULL, 10);

	SAFE_COPY(qnew->left_name, north);
	SAFE_COPY(qnew->right_name, south);

	setCDKScrollPosition(queue_list, 0);
	insertCDKScrollItem(queue_list, buffer);

	num_queues++;

}







void qlist_get_queues(void)
{

	if (!queue_list) return;

	num_queues = 0;
	engine_qlist(qlist_get_queues_sub);

}



CDKSCROLL *qlist_get_viewer(void)
{

	return msgs_viewer;

}




void qlist_draw(void)
{
	char *queue[1] = {" Getting queues... "};

	begx = 0;
	begy = 1;
	height = LINES-begy-1;
	width = COLS;


	// Refresh interval selector
	{
		char *ints_msg[] = { "</63>Never", "</63>20s", "</63>40s", "</63>1m",
							 "</63>2m",    "</63>5m",  "</63>10m" };
		
		refresh_int = newCDKItemlist(cdkscreen, 
									 LEFT, begy,
									 NULL, "</B>R<!B>efresh interval: ",
									 ints_msg, 7, 
									 current_refresh_interval, 
									 FALSE, FALSE);

		if (!refresh_int) goto fail;

		setCDKItemlistPostProcess(refresh_int, refresh_callback, NULL);
		setCDKItemlistPreProcess(refresh_int, refresh_precallback, NULL);

	}




	// Create the queue area.
	queue_list = newCDKScroll(cdkscreen,
							  begx, begy+1,
							  RIGHT,
							  height - 10, width,
							  NULL,
							  queue,
							  1,
							  TRUE, // line numbers.
							  A_BOLD,
							  TRUE,
							  FALSE);
	if (!queue_list) goto fail;


	setCDKScrollPreProcess(queue_list,  queues_callback, NULL);
	


	// Info label
	{
		char *msg[1] = { "</32>In the list window: </B>(C)<!B>lear </B>(S)<!B>ubscribe </B>(Return)<!B> Grab queue.<!32>" };
		info = newCDKLabel(cdkscreen, 
						   LEFT, height - 8,
						   msg,
						   1,
						   FALSE,
						   FALSE);
		if (!info) goto fail;

	}


	// QS Window
	{
			char *list[1] = {"<Log Window>"};
			
			msgs_viewer = newCDKScroll(cdkscreen, 
									   begx, height - 7, 
									   RIGHT,
									   8, width,
									   NULL, 
									   list,
									   1,
									   FALSE,
									   A_BOLD,
									   TRUE,
									   FALSE);
			
			if (!msgs_viewer) goto fail;
			
	}




	refreshCDKScreen(cdkscreen);



	botbar_print("Getting queues...");

	
	qlist_get_queues();

	
	my_setCDKFocusCurrent(cdkscreen, ObjPtr(queue_list));

	qlist_leave = 0;

	qlist_counter = 0;
	return;

 fail:
	qlist_undraw();
	return;

}


void qlist_undraw(void)
{

	if (queue_list) {
		destroyCDKScroll(queue_list);
		queue_list = NULL;
	}

	if (refresh_int) {
		destroyCDKItemlist(refresh_int);
		refresh_int = NULL;
	}

	if (info) {
		destroyCDKLabel(info);
		info = NULL;
	}

	if (msgs_viewer) {
		destroyCDKScroll(msgs_viewer);
		msgs_viewer = NULL;
	}


	qlist_leave = 0;

}



int qlist_checkclick(MEVENT *event)
{

	if(event->bstate & BUTTON1_CLICKED) {

	}

	return 1;
}



void qlist_poll(void)
{


	// Auto refresh counter.. increase if it isn't "never".
	if (current_refresh_seconds) {

		qlist_counter++;
		
		// Clear last message after 6 seconds.
		if (qlist_counter == 6)
			botbar_print("");
		
	// Have we gone above refresh period?
		if (qlist_counter >= current_refresh_seconds) {
			qlist_counter = 0;
			
			qlist_clear();
			qlist_get_queues();
			
		}

		// One shot refresh
		if (current_refresh_seconds < 0) {

			// Change it here, so when we call the callback, it will
			// appear as if it is changed, and be set to its new value.
			current_refresh_interval = -1;
			
			// Assign it to proper value again, we do this
			// by faking an event
			refresh_callback(vITEMLIST,
							 (void *)ObjPtr(refresh_int),
							 NULL,
							 0);

		}

	}



	switch(qlist_leave) {

	case QLIST_GOTO_DISPLAY:
		visible = VISIBLE_DISPLAY;
		visible_draw();
		display_refresh(LEFT_SID);
		display_refresh(RIGHT_SID);
		break;

	case QLIST_GOTO_QUEUEMGR:
		visible = VISIBLE_QUEUEMGR;
		visible_draw();
		break;

	}

}

