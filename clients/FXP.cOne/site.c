#include <stdio.h>

#include <ncurses.h>
#include <cdk/cdk.h>

#include "lion.h"
#include "misc.h"

#include "parser.h"
#include "main.h"
#include "site.h"
#include "engine.h"
#include "traverse.h"
#include "display.h"


static CDKLABEL     *cbox          = NULL;
static CDKENTRY     *sentry        = NULL;
static CDKSELECTION *options       = NULL;
static CDKBUTTONBOX *okbutt        = NULL;
static CDKBUTTONBOX *cancelbutt    = NULL;
static CDKBUTTONBOX *pastebutt     = NULL;
static CDKSCROLL    *msgs          = NULL;


static int begx  = 0,  begy   = 0;
static int width = 60, height = 9;

static int site_side = 0;
static int site_type = 0;

static int site_need_refresh = 0;

static int site_ok = 0;
static int site_log = 1; // Default is on.




static int okbutt_callback(EObjectType cdktype, void *object, 
						   void *clientData, chtype input)
{
	//	CDKBUTTONBOX *button = (CDKBUTTONBOX *) object;
	MEVENT event;
	
	if (input == KEY_MOUSE) 
		if (getmouse(&event) == OK) {
			return check_all_mouse(&event);
		} // get event
	
	
	if (input == KEY_ENTER)
		site_ok = 1;


	return 1;
}




static int cancelbutt_callback(EObjectType cdktype, void *object, 
							   void *clientData, chtype input)
{
	//CDKBUTTONBOX *button = (CDKBUTTONBOX *) object;
	MEVENT event;
	
	if (input == KEY_MOUSE) 
		if (getmouse(&event) == OK) {
			return check_all_mouse(&event);
		} // get event
	
	
	if (input == KEY_ENTER)
		site_ok = 2;

	return 1;
}







static int site_paste_sub(char *name)
{
	char buffer[1024];

	snprintf(buffer, sizeof(buffer), "%s %s",
			 getCDKEntryValue(sentry),
			 name);
	setCDKEntryValue(sentry, buffer);

	return 1;
}






static int pastebutt_callback(EObjectType cdktype, void *object, 
							   void *clientData, chtype input)
{
	//CDKBUTTONBOX *button = (CDKBUTTONBOX *) object;
	MEVENT event;
	
	if (input == KEY_MOUSE) 
		if (getmouse(&event) == OK) {
			return check_all_mouse(&event);
		} // get event
	
	
	if (input == KEY_ENTER) {
		display_iterate_selected(site_side, site_paste_sub, DISPLAY_ALL);
		drawCDKEntry(sentry, FALSE);
	}

	return 1;
}




static int sentry_callback(EObjectType cdktype, void *object, 
						   void *clientData, chtype input)
{
	//	CDKENTRY *entry = (CDKENTRY *) object;
	MEVENT event;
	
	if (input == KEY_MOUSE) 
		if (getmouse(&event) == OK) {
			return check_all_mouse(&event);
		} // get event
	
	
	if (input == KEY_ENTER) {
		site_ok = 1;
	}

	return 1;
}




static int colour_pair(int fg, int bg)
{
	// ANSI: Black, Red, Green, Yellow, Blue, Magenta, Cyan, White
	// CDK : White, Red, Green, Yellow, Blue, Purple, Cyan, Black

	// CDK 0 is default, 1-64 (<-inclusive) are pairs.
	if (!fg && !bg) return 0;

	switch(fg) {
	case 0:
		fg = 7;
		break;
	case 7:
		fg = 0;
		break;
	}

	switch(bg) {
	case 0:
		bg = 7;
		break;
	case 7:
		bg = 0;
		break;
	}

	return fg * 8 + bg + 1;

}



//
// Place for future convertion of ANSI colours to CDK colours.
//
// Find all "ESC [ 1 ; 37 ; 44 m" (without spaces) and replace with
// appropriate CDK colour pair. 
//
// There can be anything from "one" entry, to as many as you need separated by
// semi-colon. Although, generally you need no more than 3.
// 
// E[1m       ->  </B>
// E[1;37;44m ->  </B></5>
// E[0m       ->  <N>
/**
   1m     -     Change text to hicolour (bold) mode
   4m     -        "    "   "  Underline (doesn't seem to work)
   5m     -        "    "   "  BLINK!!
   8m     -        "    "   "  Hidden (same colour as bg)
   30m    -        "    "   "  Black
   31m    -        "    "   "  Red
   32m    -        "    "   "  Green
   33m    -        "    "   "  Yellow
   34m    -        "    "   "  Blue
   35m    -        "    "   "  Magenta
   36m    -        "    "   "  Cyan
   37m    -        "    "   "  White
   40m    -     Change Background to Black
   41m    -        "       "      "  Red
   42m    -        "       "      "  Green
   43m    -        "       "      "  Yellow
   44m    -        "       "      "  Blue
   45m    -        "       "      "  Magenta
   46m    -        "       "      "  Cyan
   47m    -        "       "      "  White
   7m     -     Change to Black text on a White bg
   0m     -     Turn off all attributes.
**/
//
char *site_ansi2CDK(char *ansi)
{
	int in, out, new_colour, last_pair;
	int fg, bg, code;
	static char result[1024];
	char *next;

	fg = 0;
	bg = 0;
	last_pair = 0;
	
	*result = 0;

	for (in = 0, out = 0; 
		 ansi[in]; 
		 in++) {

		if (out >= (sizeof(result) - 8)) break; // Near end? Bail, with room


		// Look for ANSI start
		if ((ansi[in    ] ==   27) &&  // ESC
			(ansi[in + 1] == '[')) { // [

			// Skip E[
			in+=2;

			// While we can read a number, until "m".
			do {

				code = strtoul( &ansi[ in ], &next, 10);
			
				if (next) { // How much to advance?
					in += next - &ansi[ in ];
				}

				// Deal with code here.
				switch(code) {
				case 0:  // all OFF
					strcpy( &result[ out ], "<!B>"); 
					out += 4;
					fg = 0;
					bg = 0;
					new_colour = 1;
					break;
				case 1:  // bald ON
					strcpy( &result[ out ], "</B>"); 
					out += 4;
					break;
				case 30:
				case 31:
				case 32:
				case 33:
				case 34:
				case 35:
				case 36:
				case 37:
					fg = code - 30;
					new_colour = 1;
					break;
				case 40:
				case 41:
				case 42:
				case 43:
				case 44:
				case 45:
				case 46:
				case 47:
					bg = code - 40;
					new_colour = 1;
					break;
				} // switch code

				
				// If we see a NULL, or "m" we are done.
				if ((!ansi[ in ]) ||
					(ansi[ in ] == 'm')) {
					
					if (new_colour) {
						int pair;

						pair = colour_pair(fg, bg);

						if (pair != last_pair) {

							// Disable old colour pair
							snprintf(&result[ out ],
									 sizeof(result) - out,
									 "<!%02d>",
									 last_pair
									 ); 
							out += 5;

							// Enable new pair
							snprintf(&result[ out ],
									 sizeof(result) - out,
									 "</%02d>",
									 pair
									 ); 
							out += 5;

							last_pair = pair;
						} // last_pair

					}

					new_colour = 0;
					break;
				}

				if (out >= (sizeof(result) - 8)) break; 

				// We should be pointing to a ';' here.
				in++;


			} while (ansi[ in ]);
			

		} else { // Not ANSI E[

			result[ out ] = ansi[ in ];
			out++;

		}

	} // for loop


	// Null-term
	result[ out ] = 0;

	return result;
	
}






static void site_cmd_sub(char **keys, char **values, int items)
{
	char *msg, *code, *cdk;
	int lines;
	
	if (!msgs) return;

	code    = parser_findkey(keys, values, items, "CODE");
	msg     = parser_findkey(keys, values, items, "MSG");
	if (!msg) return;

	msg = misc_url_decode(msg); // Needs to be FREEd

	cdk = site_ansi2CDK(msg);

	// Add this message
	addCDKScrollItem(msgs, cdk);

	// Set it to highest value.
	setCDKScrollPosition(msgs, 999999);
	lines = getCDKScrollCurrent(msgs);
	if (lines >= 200)
		deleteCDKScrollItem(msgs, 0);

	drawCDKScroll(msgs, TRUE);

	SAFE_FREE(msg);

}





static int site_dele_sub(char *name)
{
	int log;

	site_need_refresh = 1;

	log = getCDKSelectionChoice(options, 0); 

	engine_dele(site_side, log, name, site_cmd_sub);

	return 1;
}

static int site_rmd_sub(char *name)
{
	int log;

	site_need_refresh = 1;

	log = getCDKSelectionChoice(options, 0); 

	engine_rmd(site_side, log, name, site_cmd_sub);

	return 1;
}


static int site_send_sub(char *name)
{
	char *cmd;
	int log;
	char buffer[1024];

	log = getCDKSelectionChoice(options, 0); 
	cmd = getCDKEntryValue(sentry);

	snprintf(buffer, sizeof(buffer), cmd, name);

	engine_site(site_side, log, buffer, site_cmd_sub);

	return 1;
}



static int site_rename_sub(char *name)
{
	char *cmd;
	int log;
	char buffer[1024];

	site_need_refresh = 1;

	log = getCDKSelectionChoice(options, 0); 
	cmd = getCDKEntryValue(sentry);

	snprintf(buffer, sizeof(buffer), cmd, name);

	engine_rename(site_side, log, name, buffer, site_cmd_sub);

	return 1;
}






void site_draw(void)
{
	int i;
	char boxtop[256];
	char boxline[256];
	char boxbot[256];
	char *boxmsg[height];
	char *tmp[1];
	char *choices[2] = {"</33>[ ]", "</33>[*]" };
	char *list[1] = {"<Site output>"};

	// Screen width
	width = COLS;
	snprintf(boxtop, sizeof(boxtop), 
			 "</33><#UL><#HL(%d)><#UR><!33>", width - 2);
	snprintf(boxline, sizeof(boxline), 
			 "</33><#VL>%*.*s<#VL><!33>", 
			 width - 2, width - 2, " ");
	snprintf(boxbot, sizeof(boxbot), 
			 "</33><#LL><#HL(%d)><#LR><!33>", width - 2);
	

	for (i = 1; i < height-1; i++)
		boxmsg[i] = boxline;
	boxmsg[0] = boxtop;
	boxmsg[height-1] = boxbot;

	begx = 0;
	begy = 2;


	cbox = newCDKLabel(cdkscreen, begx, begy, boxmsg, height, FALSE, FALSE);
	if (!cbox) goto fail;

	snprintf(boxtop, sizeof(boxtop), "</33>CMD:%*.*s<!33>",
			 width-6, width-6, " ");


	// Only draw the Entry box in some situations.
	if ((site_type == SITE_CMD) ||
		(site_type == SITE_MKD) ||
		(site_type == SITE_RENAME)) {
		
		sentry = newCDKEntry(cdkscreen, begx+1, begy+1,
							 boxtop, "", 0, ' ', vMIXED, 
							 width-2, 1, 1024, FALSE, FALSE);
		
		if (!sentry) goto fail;
		
		if (site_type == SITE_CMD)
			setCDKEntryValue(sentry, "WHO"); 
		else if (site_type == SITE_MKD)
			setCDKEntryValue(sentry, "New Folder"); 
		else if (site_type == SITE_RENAME)
			setCDKEntryValue(sentry, "%s-new"); 


		setCDKEntryPreProcess(sentry, sentry_callback, NULL);

	}

	

	snprintf(boxtop, sizeof(boxtop), "</33>%-*.*s<!33>",
			 width-5, width-5, "Verbose output"); // -"[ ]" -borders
	tmp[0] = boxtop;

	options = newCDKSelection(cdkscreen, 
							  begx+1, begy+3, NONE,
							  1,
							  width - 2,
							  NULL,
							  tmp, 
							  1,
							  choices,
							  2,
							  0,
							  FALSE,
							  FALSE);
	if (!options) goto fail;
							  

	// Set LOG to last value.
	setCDKSelectionChoice(options, 0, site_log); 


	
	snprintf(boxtop, sizeof(boxtop), "</5>  OK  </!5>");

	okbutt = newCDKButtonbox(cdkscreen,
							 begx+3, begy+5,  // x y
							 1, 10,           // width height
							 NULL,
							 1, 1,            // rows cols
							 tmp, 1,
							 A_BOLD,
							 TRUE,
							 FALSE);
	if (!okbutt) goto fail;

	setCDKButtonboxBackgroundColor(okbutt, "</33>");

	setCDKButtonboxPreProcess(okbutt, okbutt_callback, NULL);





	snprintf(boxtop, sizeof(boxtop), "</5>Cancel</!5>");

	cancelbutt = newCDKButtonbox(cdkscreen,
								 begx+16, begy+5, // x y
								 1, 10,           // width height
								 NULL,
								 1, 1,            // rows cols
								 tmp, 1,
								 A_BOLD,
								 TRUE,
								 FALSE);
	if (!cancelbutt) goto fail;

	setCDKButtonboxBackgroundColor(cancelbutt, "</33>");
	setCDKButtonboxPreProcess(cancelbutt, cancelbutt_callback, NULL);





	snprintf(boxtop, sizeof(boxtop), " </5> Paste </!5>");

	pastebutt = newCDKButtonbox(cdkscreen,
								begx+29, begy+5, // x y
								1, 10,           // width height
								NULL,
								1, 1,            // rows cols
								tmp, 1,
								A_BOLD,
								TRUE,
								FALSE);
	if (!pastebutt) goto fail;
	
	setCDKButtonboxBackgroundColor(pastebutt, "</33>");
	setCDKButtonboxPreProcess(pastebutt, pastebutt_callback, NULL);




	msgs = newCDKScroll(cdkscreen, 
						LEFT, begy+height,
						RIGHT,
						LINES-height-3, width,
						NULL, 
						list,
						1,
						FALSE,
						A_BOLD,
						//A_REVERSE,
						TRUE,
						FALSE);
	
	if (!msgs) goto fail;






	botbar_print("Enter SITE cmd. For multiple selections, use %s as marker. ");

	display_disable_focus();

	if (sentry)
		my_setCDKFocusCurrent(cdkscreen, ObjPtr(sentry));
	else
		my_setCDKFocusCurrent(cdkscreen, ObjPtr(okbutt));

	refreshCDKScreen(cdkscreen);
	return;


 fail:
	site_undraw();
	return;

}



void site_undraw(void)
{

	if (cbox) {
		destroyCDKLabel(cbox);
		cbox = NULL;
	}

	if (sentry) {
		setCDKEntryBackgroundColor(sentry, "</0>");
		drawCDKEntry(sentry, FALSE);
		destroyCDKEntry(sentry);
		sentry = NULL;
	}

	if (options) {
		destroyCDKSelection(options);
		options = NULL;
	}

	if (okbutt) {
		setCDKEntryBackgroundColor(okbutt, "</0>");
		drawCDKEntry(okbutt, FALSE);
		destroyCDKButtonbox(okbutt);
		okbutt = NULL;
	}

	if (cancelbutt) {
		setCDKEntryBackgroundColor(cancelbutt, "</0>");
		drawCDKEntry(cancelbutt, FALSE);
		destroyCDKButtonbox(cancelbutt);
		cancelbutt = NULL;
	}

	if (pastebutt) {
		setCDKEntryBackgroundColor(pastebutt, "</0>");
		drawCDKEntry(pastebutt, FALSE);
		destroyCDKButtonbox(pastebutt);
		pastebutt = NULL;
	}

	if (msgs) {
		destroyCDKScroll(msgs);
		msgs = NULL;
	}

	display_enable_focus();

	refreshCDKScreen(cdkscreen);

	site_type = 0;
}




int site_checkclick(MEVENT *event)
{

	if(event->bstate & BUTTON1_CLICKED) { 
		
		if (wenclose(okbutt->win, event->y, event->x)) {
			my_setCDKFocusCurrent(cdkscreen, ObjPtr(okbutt));
			ungetch(KEY_ENTER);
			return 0;
		}

		if (wenclose(cancelbutt->win, event->y, event->x)) {
			my_setCDKFocusCurrent(cdkscreen, ObjPtr(cancelbutt));
			ungetch(KEY_ENTER);
			return 0;
		}

		if (wenclose(pastebutt->win, event->y, event->x)) {
			my_setCDKFocusCurrent(cdkscreen, ObjPtr(pastebutt));
			ungetch(KEY_ENTER);
			return 0;
		}

		if (wenclose(sentry->win, event->y, event->x)) {
			my_setCDKFocusCurrent(cdkscreen, ObjPtr(sentry));
			return 0;
		}

		if (wenclose(options->win, event->y, event->x)) {
			my_setCDKFocusCurrent(cdkscreen, ObjPtr(options));
			ungetch(' ');
			return 0;
		}

	}

	return 1;
}



void site_poll(void)
{
	char *cmd;
	int log;

	switch(site_ok) {

	case 1: // OK button pushed, send the command
		// We are to send the command, so check if we have a
		// "%s" in the string, if so, we iterate the command
		// against all selected items. Otherwise, just send it.

		// Remember the LOG setting.
		site_log = 	getCDKSelectionChoice(options, 0); 

		switch( site_type ) {

		case SITE_CMD:
			cmd = getCDKEntryValue(sentry);
			log = getCDKSelectionChoice(options, 0); 
			
			if (!strstr(cmd, "%s")) 
				engine_site(site_side, log, cmd, site_cmd_sub);
			else 
				display_iterate_selected(site_side, site_send_sub, DISPLAY_ALL);
			break;

		case SITE_DELE:
			site_ok = 2;
			return;
			break;


		case SITE_MKD:
			cmd = getCDKEntryValue(sentry);
			log = getCDKSelectionChoice(options, 0); 
			
			site_need_refresh = 1;
			engine_mkd(site_side, log, cmd, site_cmd_sub);
			break;

		case SITE_RENAME:
			cmd = getCDKEntryValue(sentry);
			log = getCDKSelectionChoice(options, 0); 

			display_iterate_selected(site_side, site_rename_sub, DISPLAY_ALL);
			break;


		}
		break;
		
	case 2: // Cancel button pushed, just exit.
		visible &= ~VISIBLE_SITE;
		visible_draw();

		// Should we issue refresh?
		if (site_need_refresh) 
			display_refresh(site_side);

		site_need_refresh = 0;
		break;

	}

	site_ok = 0;

}









void site_cmd(int side)
{
	// We can only issue site commands if we are in Display mode, and 
	// connected.
	if (!(visible & VISIBLE_DISPLAY)) return;
	if (visible & VISIBLE_SITE) return;
	if (!engine_isconnected(side)) return;

	site_side = side;
	site_type = SITE_CMD;

	visible |= VISIBLE_SITE;
	visible_draw();

}


void site_dele(int side)
{
	// We can only issue site commands if we are in Display mode, and 
	// connected.
	if (!(visible & VISIBLE_DISPLAY)) return;
	if (visible & VISIBLE_SITE) return;
	if (!engine_isconnected(side)) return;

	site_side = side;
	site_type = SITE_DELE;

	visible |= VISIBLE_SITE;
	visible_draw();

	display_iterate_selected(site_side, site_dele_sub, DISPLAY_ONLYFILES);
	display_iterate_selected(site_side, site_rmd_sub,  DISPLAY_ONLYDIRS);

}




void site_mkd(int side)
{
	// We can only issue site commands if we are in Display mode, and 
	// connected.
	if (!(visible & VISIBLE_DISPLAY)) return;
	if (visible & VISIBLE_SITE) return;
	if (!engine_isconnected(side)) return;

	site_side = side;
	site_type = SITE_MKD;

	visible |= VISIBLE_SITE;
	visible_draw();

}


void site_rename(int side)
{
	// We can only issue site commands if we are in Display mode, and 
	// connected.
	if (!(visible & VISIBLE_DISPLAY)) return;
	if (visible & VISIBLE_SITE) return;
	if (!engine_isconnected(side)) return;

	site_side = side;
	site_type = SITE_RENAME;

	visible |= VISIBLE_SITE;
	visible_draw();

}
