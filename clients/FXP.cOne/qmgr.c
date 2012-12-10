#if HAVE_CONFIG_H
#include <config.h>
#endif

#if HAVE_STDARG_H
#  include <stdarg.h>
#  define VA_START(a, f)        va_start(a, f)
#else
#  if HAVE_VARARGS_H
#    include <varargs.h>
#    define VA_START(a, f)      va_start(a)
#  endif
#endif
#ifndef VA_START
#  warning "no variadic api"
#endif


#include <stdio.h>
#include <cdk/cdk.h>

#include "lion.h"
#include "misc.h"

#include "traverse.h"
#include "main.h"
#include "engine.h"
#include "parser.h"
#include "display.h"
#include "qlist.h"
#include "qmgr.h"

/**
-.--------------------------------------------------------------------------.-
+----------------------------------------------------------------------------+
|/tv/Some.Release.tv-GRP => /dir/path/Some.Release.tv-GRP                   ^|
|/vcd/Some.Great.Movie-VCD => /movies/mpg/Some.Great.Movie-VCD              ||
|                                                                           ||
|                                                                           ||
|                                                                           ||
|                                                                           ||
|                                                                           ||
|                                                                           ||
|                                                                           ||
|                                                                           ||
|                                                                           ||
|                                                                           ||
|                                                                           v|
+----------------------------------------------------------------------------+
+----------------------------------------------------------------------------+
|some.release.part05.rar: src 425 PASV Connection Failed.                   ^|
|some.release.part06.rar: dst 553 Can't open for writing                    ||
|                                                                           v|
|                                                                           v|
+----------------------------------------------------------------------------+
/tv/Some.Release.tv-GRP/some.release.part07.rar
/dir/path/Some.Release.tv-GRP/some.release.part07.rar
Directory      54945        Jun 21 17:02
[Dupe] [Del] [-Up] [+Down] [Top] [Bot] [Edit]
--FXP.One--|------------------------------------------------------------------
**/

/**
**/


/**
-.--------------------------------------------------------------------------.-
Queue: [active] [Dupe] [Del] [-Up] [+Down] [Top] [Bot] [Edit]
+----------------------------------------------------------------------------+
|/tv/Some.Release.tv-GRP/some.file.part03.rar => /dir/path/Some.Release.tv- ^|
|/tv/Some.Release.tv-GRP/some.file.part04.rar => /dir/path/Some.Release.tv- ||
|/tv/Some.Release.tv-GRP/some.file.part05.rar => /dir/path/Some.Release.tv- ||
|/tv/Some.Release.tv-GRP/ => /dir/path/Some.Release.tv-GRP/                 ||
|/vcd/Some.Great.Movie-VCD/ => /movies/mpg/Some.Great.Movie-VCD/            ||
|                                                                           ||
|                                                                           ||
|                                                                           ||
|                                                                           ||
|                                                                           ||
|                                                                           ||
|                                                                           v|
+----------------------------------------------------------------------------+
S: /tv/Some.Release.tv-GRP/some.release.part03.rar                  (15000000)
D: /dir/path/Some.Release.tv-GRP/some.release.part03.rar                   (0)
+----------------------------------------------------------------------------+
|Starting on item 'some.file.part01.rar'                                    ^|
|Transfer started (SSL), resuming at 542355                                 ||
|Transfer finished.                                  00:00:18s at   786KB/s ||
|Starting on item 'some.file.part02.rar'                                    ||
|Transfer started (SSL)                                                     ||
|                                                                           ||
|                                                                           v|
+----------------------------------------------------------------------------+
Queue: 14 Directories, 22 Files, 4.5GB total. ETA of files: 22m, 18s.
--FXP.One--|------------------------------------------------------------------
**/



static CDKSCROLL           *msgs_viewer   = NULL;
static CDKSCROLL           *queue_viewer  = NULL;
static CDKITEMLIST         *active_queue  = NULL;
static CDKLABEL            *help          = NULL;
static CDKLABEL            *eta           = NULL;

static int begx, begy, height, width;

static int current_active     =  0;

// While we are getting a queue, this is set, so we don't issue
// new gets while one is processing.
static int get_active = 0;

static int qmgr_goto_display = 0;






static int active_precallback(EObjectType cdktype, void *object, 
							  void *clientData, chtype input)
{
	CDKITEMLIST *path = (CDKITEMLIST *)object;
	MEVENT event;
	int current;

	if (input == KEY_MOUSE) 
		if (getmouse(&event) == OK) 
			return check_all_mouse(&event);
	
	switch(input) {
	case KEY_ENTER:
		current = getCDKItemlistCurrentItem(path);
		
		if (current != current_active) {
			current_active = current;
		}
		
		qmgr_get_queue();
		break;
	}

	return 1;
}


static int active_callback(EObjectType cdktype, void *object, 
						   void *clientData, chtype input)
{

	return 1;
}




//
// Due to some missing API calls in CDKSCROLL this function is rather
// ugly.
// Moves a line in SCROLL to another. Alas, we lose ALL formatting.
//
static void qmgr_move_item(unsigned int nfrom, unsigned int nat)
{
	char *str;
	int current;

	// We should check that from isn't beyond the GUI.. no API
	if (nfrom >= queue_viewer->listSize) return;

	// We have no API for moving things around, nor is there a
	// way to get the selected item (we can only get ALL items).
	// Also! Much worse, it leaves the line-number part as part of
	// the string!
	str = strdup(chtype2Char(queue_viewer->item[nfrom]));
	if (!str) return;
	
	// Remember selected position, incase we use insert.
	current = getCDKScrollCurrent(queue_viewer);

	// Delete the current item
	deleteCDKScrollItem(queue_viewer, nfrom);
	
	// Re-insert it
	if (nat >= queue_viewer->listSize) {
		addCDKScrollItem(queue_viewer, &str[6]);
		setCDKScrollCurrent(queue_viewer, current);
	} else {
		setCDKScrollCurrent(queue_viewer, nat);
		insertCDKScrollItem(queue_viewer, &str[6]);
		setCDKScrollCurrent(queue_viewer, current);
	}
	SAFE_FREE(str);

	// Redraw
	drawCDKScroll(queue_viewer, TRUE);
}




static void qmgr_qmove_sub(char **keys, char **values, int items)
{
	char *from, *at;
	unsigned int nfrom, nat;

	// QMOVE|QID=1|@=0|FROM=5|CODE=0|ITEMS=3|MSG=OK
	from = parser_findkey(keys, values, items, "FROM");
	at   = parser_findkey(keys, values, items, "@");

	if (!from || !at) return;

	nfrom = strtoul(from, NULL, 10);
	nat   = strtoul(at, NULL, 10);

	qmgr_move_item(nfrom, nat);

}


static void qmgr_qdel_sub(char **keys, char **values, int items)
{
	char *at;
	unsigned int nat;

	at   = parser_findkey(keys, values, items, "@");

	if (!at) return;

	nat   = strtoul(at, NULL, 10);

	deleteCDKScrollItem(queue_viewer, nat);
	drawCDKScroll(queue_viewer, TRUE);

}






static int queue_callback(EObjectType cdktype, void *object, 
						  void *clientData, chtype input)
{
	CDKSCROLL *path = (CDKSCROLL *)object;
	int current;

	MEVENT event;

	if (input == KEY_MOUSE) 
		if (getmouse(&event) == OK) 
			return check_all_mouse(&event);
	

	// Keys apply only for active queue, not qerr
	if (current_active) return 1;

	current = getCDKScrollCurrent(path);

	switch(input) {

	case '-':  // Move item up.
		if (current) {
			engine_qmove(current, current-1, qmgr_qmove_sub);
			// We try to predict where it will go, so multi-key presses work
			setCDKScrollCurrent(path, current-1);
		}
		break;

	case '+':  // Move item down.
		if (current<path->listSize) {
			engine_qmove(current, current+1, qmgr_qmove_sub);
			// We try to predict where it will go, so multi-key presses work
			setCDKScrollCurrent(path, current+1);
		}
		break;

	case 't':  // Move item to TOP
	case 'T':  
		if (current) {
			engine_qmove(current, -1, qmgr_qmove_sub);
			// We try to predict where it will go, so multi-key presses work
			setCDKScrollCurrent(path, 0);
		}
		break;

	case 'b':  // Move item to BOT
	case 'B':  
		if (current < path->listSize) {
			engine_qmove(current, -2, qmgr_qmove_sub);
			// We try to predict where it will go, so multi-key presses work
			setCDKScrollCurrent(path, path->listSize - 1);
		}
		break;

	case KEY_DC:  // Move item to BOT
	case 'd':
		engine_qdel(current, qmgr_qdel_sub);
		break;

	}

	return 1;
}



















char *basename(char *s)
{
	char *r;

	r = strrchr(s, '/');
	if (r) return &r[1];

	return s;
}






#if HAVE_STDARG_H
int qmgr_printf(char const *fmt, ...)
#else
int qmgr_printf(fmt, va_alist)
     char const *fmt;
     va_dcl
#endif
{
	va_list ap;
	char msg[1024];
	int result, lines;
	CDKSCROLL *view;

	// We hi-jack this fucntion from qlist as well,
	// if the local msgs_viewer doesn't exist, check if qlist's one
	// does. If so, send the output to it instead.
	if (msgs_viewer)
		view = msgs_viewer;
	else
		view = qlist_get_viewer();

	if (!view) 
		return -1;


	VA_START(ap, fmt);
	
	result = vsnprintf(msg, sizeof(msg), fmt, ap);
	va_end(ap);
	

	addCDKScrollItem(view, msg);

	// Set it to highest value.
	setCDKScrollPosition(view, 999999);
	lines = getCDKScrollCurrent(view);
	if (lines >= 200)
		deleteCDKScrollItem(view, 0);

	drawCDKScroll(view, TRUE);

	return result;
}



void qmgr_qs(char **keys, char **values, int items)
{
	char *key, *tmp, *tmp2, *tmp3;

	// Are we displaying?

	// Which type of QS is this?
	// MKD, SRCCWD, DSTCWD, START, EQUALSIZE, XFRACT, XFREND, FAILED
	key  = parser_findkey(keys, values, items, "MKD");
	if (key) {
		qmgr_printf("Created destination directory '%s'", key);
		return;
	}

	key  = parser_findkey(keys, values, items, "SRCCWD");
	if (key) {
		key = misc_url_decode(key);
		qmgr_printf("Source CWD to '%s'", key);
		SAFE_FREE(key);
		return;
	}

	key  = parser_findkey(keys, values, items, "DSTCWD");
	if (key) {
		key = misc_url_decode(key);
		qmgr_printf("Destination CWD to '%s'", key);
		SAFE_FREE(key);
		return;
	}

	key  = parser_findkey(keys, values, items, "START");
	tmp  = parser_findkey(keys, values, items, "SRCPATH");

	if (key && tmp) {
		tmp = misc_url_decode(tmp);
		qmgr_printf("Considering next item '%s'", basename(tmp));
		SAFE_FREE(tmp);
		return;
	}

	key  = parser_findkey(keys, values, items, "EQUALSIZE");

	if (key) {
		qmgr_printf("Identical size on both source and destination, next!");
		return;
	}

	key  = parser_findkey(keys, values, items, "XFRACT");
	tmp  = parser_findkey(keys, values, items, "SECURE");
	tmp2 = parser_findkey(keys, values, items, "RESUME");
	tmp3 = parser_findkey(keys, values, items, "PASV");

	if (key && tmp) {
		int sec;
		sec = mystrccmp(tmp, "YES");
		qmgr_printf("Transfer started... (%s) %s%s [PASV=%s]",
					!sec ? "Secure" : "</16></B>INSECURE<!B><!16>",
					!tmp2 ? "" : "Resuming from ",
					!tmp2 ? "" : tmp2,
					tmp3 ? tmp3 : "unknown");
		return;
	}

	key  = parser_findkey(keys, values, items, "XFREND");
	tmp  = parser_findkey(keys, values, items, "TIME");
	tmp2 = parser_findkey(keys, values, items, "KB/s");
	if (key) {
		int edge = width-20-2-10-10-1-2;
		
		qmgr_printf("Transfer completed. %*.*s %10.10s %10.10s",
					edge, edge, " ",
					tmp ? tmp : " ",
					tmp2 ? tmp2 : " ");
					
		return;
	}


	key  = parser_findkey(keys, values, items, "FAILED");
	tmp  = parser_findkey(keys, values, items, "SERR");
	tmp2 = parser_findkey(keys, values, items, "DERR");
	tmp3 = parser_findkey(keys, values, items, "COUNT");

	if (key && tmp3) {

		qmgr_printf("Item failed: %s. (Count %s)",
					tmp ? tmp : tmp2 ? tmp2 : "unknown",
					tmp3);
		return;
	}

}








static void qmgr_get_queue_sub(char **keys, char **values, int items)
{
	char *at, *type, *srcpath, *dstpath, *srcsize;
	int isdir = 0;
	char buffer[1024];
	unsigned int pos;

	if (!items) {
		// We're done

		setCDKScrollPosition(queue_viewer, 0); // View TOP
		drawCDKScroll(queue_viewer, TRUE);     // Draw it
		get_active = 0;
		return;

	}

	// " QGET|QID=1|@=0|FTYPE=FILE|SRC=NORTH|SRCPATH=/Giana_Sisters_GBA/gfx_blocks.txt|SRCSIZE=9328|SRCREST=0|DSTPATH=/tmp//gfx_blocks.txt|DSTSIZE=0|DSTREST=0"
	
	at    = parser_findkey(keys, values, items, "@");
	type  = parser_findkey(keys, values, items, "QTYPE");
	//src   = parser_findkey(keys, values, items, "SRC");
	srcpath = parser_findkey(keys, values, items, "SRCPATH");
	dstpath = parser_findkey(keys, values, items, "DSTPATH");
	srcsize = parser_findkey(keys, values, items, "SRCSIZE");

	if (!at || !type || !srcpath || !dstpath || !srcsize)
		return;

	if (!mystrccmp("DIRECTORY", type)) 
		isdir = 1;

	srcpath = misc_url_decode(srcpath);
	dstpath = misc_url_decode(dstpath);

	if (isdir)
		snprintf(buffer, sizeof(buffer), "d </8>%s/<!8> => </8>%s/<!8>",
				 srcpath,
				 dstpath);
	else
		snprintf(buffer, sizeof(buffer), "f </32>%s<!32> => </32>%s<!32> (%s)",
				 basename(srcpath),
				 basename(dstpath),
				 srcsize);

	SAFE_FREE(srcpath);
	SAFE_FREE(dstpath);

	pos = strtoul(at, NULL, 10);

	setCDKScrollPosition(queue_viewer, pos);

	if (getCDKScrollCurrent(queue_viewer) != pos)
		addCDKScrollItem(queue_viewer, buffer);
	else
		insertCDKScrollItem(queue_viewer, buffer);

}





static void qmgr_get_err_sub(char **keys, char **values, int items)
{
	char *type, *srcpath, *dstpath, *srcerr, *dsterr;
	int isdir = 0;
	char buffer[1024], err[24];
	unsigned int i;

	if (!items) {
		// We're done

		setCDKScrollPosition(queue_viewer, 0); // View TOP
		drawCDKScroll(queue_viewer, TRUE);     // Draw it
		get_active = 0;
		return;

	}

	// "QERR|QID=1|TYPE=FILE|SRC=NORTH|SRCPATH=/Giana_Sisters_GBA//gfx_blocks.txt|DSTPATH=/tmp/tmp//gfx_blocks.txt|DERR_0=553 gfx_blocks.txt: Can't open for writing"

	type  = parser_findkey(keys, values, items, "QTYPE");
	if (!type)
		type  = parser_findkey(keys, values, items, "TYPE");

	srcpath = parser_findkey(keys, values, items, "SRCPATH");
	dstpath = parser_findkey(keys, values, items, "DSTPATH");
	
	if (!type || !srcpath || !dstpath)
		return;

	srcpath = misc_url_decode(srcpath);
	dstpath = misc_url_decode(dstpath);

	if (!mystrccmp("DIRECTORY", type)) 
		isdir = 1;

	
	// For all error messages we find...
	for (i = 0; 1 ; i++ ) {

		snprintf(err, sizeof(err), "SERR_%u", i);
		srcerr = parser_findkey(keys, values, items, err);
		err[0] = 'D';
		dsterr = parser_findkey(keys, values, items, err);
		
		if (!srcerr && !dsterr) break;

		if (srcerr) {
			
			if (isdir)
				snprintf(buffer, sizeof(buffer), "d </8>%s/<!8> => </8>%s/<!8>",
						 srcpath,
						 dstpath);
			else
				snprintf(buffer, sizeof(buffer), "f </32>%s<!32> => </32>%s<!32>",
						 basename(srcpath),
						 basename(dstpath));
			
			addCDKScrollItem(queue_viewer, buffer);

			snprintf(buffer, sizeof(buffer), "   ..SRC:%s ",
					 srcerr);

			addCDKScrollItem(queue_viewer, buffer);

		}

		if (dsterr) {
			
			if (isdir)
				snprintf(buffer, sizeof(buffer), "d </8>%s/<!8> => </8>%s/<!8>",
						 srcpath,
						 dstpath);
			else
				snprintf(buffer, sizeof(buffer), "f </32>%s<!32> => </32>%s<!32>",
						 basename(srcpath),
						 basename(dstpath));
			
			addCDKScrollItem(queue_viewer, buffer);

			snprintf(buffer, sizeof(buffer), "   ..DST:%s ",
					 dsterr);

			addCDKScrollItem(queue_viewer, buffer);

		}


	} // for i

	SAFE_FREE(srcpath);
	SAFE_FREE(dstpath);

}




static void qmgr_qgrab_sub(int sub, char *msg)
{

	switch(sub) {

	case 1: // qgrab OK
		botbar_print("QGRAB Successful");

		// This is naughty. User has already picked Display from menu
		// and we've "undraw". But we turn it on here, so we get our
		// poll() run, so we can change to Display, call draw, then leave.
		visible = VISIBLE_QUEUEMGR;
		qmgr_goto_display = 1;

		break;

	case 2: // subscribe OK
	case 0: // failed
		botbar_printf("QGRAB failed: %s", msg ? msg : "unknown error");

		// What screen?
		visible = 0;
		visible_draw();

		break;

	}

}







void qmgr_get_queue(void)
{
	char *queue[1] = {"<C><Getting queue...>"};

	if (!queue_viewer) return;

	// Clear it first.
	setCDKScrollItems(queue_viewer,
					  queue,
					  1,
					  TRUE);

	get_active = 1;

	// Delete the first line in queue
	deleteCDKScrollItem(queue_viewer, 0);

	
	if (!current_active)
		engine_qget(qmgr_get_queue_sub);
	else
		engine_qerr(qmgr_get_err_sub);


}



void qmgr_qc(char **keys, char **values, int items)
{
	char *key;
	char *at;
	unsigned int pos;

	// Are we displaying...
	if (!queue_viewer)
		return;
	
	// Displaying error queue, do nothing
	if (current_active)
		return;

	// Which type of QC is this?
	// REMOVE, MOVE, EMPTY, INSERT
	key  = parser_findkey(keys, values, items, "EMPTY");
	if (key) {
		qmgr_printf("Queue is empty, all done.");
		botbar_print(" ");
		return;
	}

	key  = parser_findkey(keys, values, items, "REMOVE");
	if (key) {

		at  = parser_findkey(keys, values, items, "@");
		if (at) {
			pos = strtoul(at, NULL, 10);
			
			deleteCDKScrollItem(queue_viewer, pos);
			drawCDKScroll(queue_viewer, TRUE);
		}

		return;
	}

	key  = parser_findkey(keys, values, items, "MOVE");
	if (key) {

		at  = parser_findkey(keys, values, items, "FROM");
		key = parser_findkey(keys, values, items, "TO");
		
		if (at && key) {
			qmgr_printf("Requeueing item last for re-think.");
			qmgr_move_item(
						   strtoul( at, NULL, 10), 
						   strtoul(key, NULL, 10));
		}
		return;
	}

	key  = parser_findkey(keys, values, items, "INSERT");
	if (key) {
		qmgr_get_queue_sub(keys, values, items);
		drawCDKScroll(queue_viewer, TRUE);
		return;
	}

}





void qmgr_eta_print(unsigned int dirs, unsigned int files,
					float gigs, float cps)
{
	char buf[1024];
	char *msg[1];
	unsigned int days, hours, mins, secs;

	if (!eta) return;

	days = 0;
	hours = 0;
	mins = 0;
	secs = 0;

	snprintf(buf, sizeof(buf), 
			 "Queue: %5u Dir%s, %5u File%s, %8.1g GB.    ETA of file%s:  %2ud %02u:%02u:%02u",
			 dirs,   dirs == 1 ? " " : "s",
			 files, files == 1 ? " " : "s",
			 gigs,  files == 1 ? " " : "s",
			 days,
			 hours,
			 mins,
			 secs);

	msg[0] = buf;
	setCDKLabelMessage(eta, msg, 1);

}





void qmgr_draw(void)
{
	char *list[1] = {"<Log Window>"};
	char *queue[1] = {"<C><Getting queue...>"};


	begx = 0;
	begy = 1;
	height = LINES-begy-1;
	width = COLS;


	// Create the queue area.
	queue_viewer = newCDKScroll(cdkscreen,
								begx, begy+1,
								RIGHT,
								height - 16, width,
								NULL,
								queue,
								1,
								TRUE, // line numbers.
								A_BOLD,
								TRUE,
								FALSE);
	if (!queue_viewer) goto fail;


	setCDKScrollPreProcess(queue_viewer, queue_callback, NULL);



	// Create the message area.
	msgs_viewer = newCDKScroll(cdkscreen, 
							   begx, height - 10, 
							   RIGHT,
							   10, width,
							   NULL, 
							   list,
							   1,
							   FALSE,
							   A_BOLD,
							   TRUE,
							   FALSE);

	if (!msgs_viewer) goto fail;





	{
		char *active_msgs[2] = {
			"</63>active", "</63>error" };

		active_queue = newCDKItemlist(cdkscreen, 
									  LEFT, begy, 
									  NULL, "</B>Q<!B>ueue: ",
									  active_msgs, 2, 
									  current_active, 
									  FALSE, FALSE);
		if (!active_queue) goto fail;

		setCDKItemlistPreProcess(active_queue, active_precallback, NULL);
		setCDKItemlistPostProcess(active_queue, active_callback, NULL);

	}




	{
		char *msg[] = {"</32>(</B>[C]<!B>opy)  </B>[D]<!B>el  </B>[-]<!B>Up  </B>[+]<!B>Down  </B>[T]<!B>op  </B>[B]<!B>ot  (</B>[E]<!B>dit)" };

		help = newCDKLabel(cdkscreen,
						   RIGHT, begy,
						   msg,
						   1,
						   FALSE,
						   FALSE);
		if (!help) goto fail;

	}


	{
		char *msg[] = {"                                                                                "};

		eta = newCDKLabel(cdkscreen,
						  LEFT,
						  height,
						  msg,
						  1,
						  FALSE,
						  FALSE);

		if (!eta) goto fail;

		qmgr_eta_print(0, 0, 0.0, 0.0);

	}












	qmgr_printf("Welcome to Queue Manager for QID %u (%s <=> %s)",
				engine_current_qid(),
				engine_get_sitename(LEFT_SID),
				engine_get_sitename(RIGHT_SID));



	refreshCDKScreen(cdkscreen);


	
	qmgr_get_queue();

	
	my_setCDKFocusCurrent(cdkscreen, ObjPtr(queue_viewer));


	return;

 fail:
	qmgr_undraw();
	return;

}


void qmgr_undraw(void)
{

	if (msgs_viewer) {
		destroyCDKScroll(msgs_viewer);
		msgs_viewer = NULL;
	}

	if (queue_viewer) {
		destroyCDKScroll(queue_viewer);
		queue_viewer = NULL;
	}

	if (help) {
		destroyCDKLabel(help);
		help = NULL;
	}

	if (active_queue) {
		destroyCDKItemlist(active_queue);
		active_queue = NULL;
	}

	if (eta) {
		destroyCDKLabel(eta);
		eta = NULL;
	}






	// Going to DISPLAY?
	if (visible & VISIBLE_DISPLAY) {

		// If neither is connected, AND we have a queue, attempt to grab
		if (!engine_isconnected(LEFT_SID) &&
			!engine_isconnected(RIGHT_SID) &&
			engine_hasqueue()) {

			// We've already turned on DISPLAY, andcalled _draw(). This
			// means display_draw() is called before qgrab can return.
			// Stop it from calling display.
			visible = 0;

			// This could be called multiple times.
			engine_qgrab(0, 0, qmgr_qgrab_sub);


		}

	}

}



int qmgr_checkclick(MEVENT *event)
{

	if(event->bstate & BUTTON1_CLICKED) {

	}

	return 1;
}



void qmgr_poll(void)
{

	if (qmgr_goto_display) {
		qmgr_goto_display = 0;

		visible = VISIBLE_DISPLAY;
		visible_draw();

		display_refresh(LEFT_SID);
		display_refresh(RIGHT_SID);
	}

}




