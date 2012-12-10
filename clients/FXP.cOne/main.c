
#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>

#include <ncurses.h>
#include <cdk/cdk.h>

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



#include "main.h"
#include "connect.h"
#include "engine.h"
#include "sitemgr.h"
#include "traverse.h"
#include "display.h"
#include "qmgr.h"
#include "qlist.h"
#include "site.h"


#define PRINT(X) if (d) fprintf(d, X);

#define NUM_MENUS   3



unsigned int do_quit = 0;
FILE *d = NULL;


       CDKSCREEN   *cdkscreen = NULL;
static CDKLABEL    *topbar    = NULL;
static CDKLABEL    *botbar    = NULL;
static WINDOW      *screen    = NULL;
static char *menulist[MAX_MENU_ITEMS][MAX_SUB_ITEMS];
static CDKMENU     *menu      = NULL;
static int submenusize[NUM_MENUS];

unsigned int visible   = 0;

#define MOUSE_MASK BUTTON1_CLICKED|BUTTON1_DOUBLE_CLICKED
//#define MOUSE_MASK 0
static int mouse_set = 0;




void botbar_print(char *line)
{
	int y,x;
	static int last_y = 0;
	char buffer[1024];
	char *botmsg[1];

	if (!botbar || !menu) return;
	
	getmaxyx(screen, y, x);

	if (last_y && (last_y != y)) {
		moveCDKLabel(botbar, LEFT, BOTTOM, FALSE, FALSE);
	}
	last_y = y;

	snprintf(buffer, sizeof(buffer), "<L></5></B>%-9.9s<#VL>%-*.*s<!5><!B>",
			 "FXP.One", x-10, x-10, line);
	
	botmsg[0] = buffer;
	setCDKLabelMessage(botbar, botmsg, 1);

	// I'd like to update the botbar placement if screen-size changed.
	//moveCDKLabel(botbar, LEFT, BOTTOM, FALSE, TRUE);


	// This might raise the bar over the menu, so we force menu to be on top
	//raiseCDKObject(vMENU, menu);
	//drawCDKMenu(menu, TRUE);
}


#if HAVE_STDARG_H
int botbar_printf(char const *fmt, ...)
#else
int botbar_printf(fmt, va_alist)
     char const *fmt;
     va_dcl
#endif
{
	va_list ap;
	char msg[1024];
	int result;

	VA_START(ap, fmt);
	
	result = vsnprintf(msg, sizeof(msg), fmt, ap);
	va_end(ap);

	botbar_print(msg);

	return result;
}





int menu_checkclick(MEVENT *event)
{
	int m;

	// Was mouse click in menus?
	if(event->bstate & BUTTON1_CLICKED) {
		for (m = 0; m < NUM_MENUS; m++) {
			if (!menu->titleWin[m] || !menu->pullWin[m]) continue;
			if (wenclose(menu->titleWin[m], event->y, event->x)) {
				setCDKMenuCurrentItem(menu, m, 0);
				ungetch(KEY_ESC);
				return 0;
			}
		}
	}

	return 0;
}


//
// All mouse events, in all inputs, come through here.
//
int check_all_mouse(MEVENT *event)
{

	if (visible & VISIBLE_SITEMGR) 
		if (!sitemgr_checkclick(event)) return 0;
	if (visible & VISIBLE_EDITSITE) 
		if (!sitemgr_checkclick(event)) return 0;
	if (visible & VISIBLE_CONNECT)
		if (!connect_checkclick(event)) return 0;
	if (visible & VISIBLE_DISPLAY)
		if (!display_checkclick(event)) return 0;
	if (visible & VISIBLE_QUEUEMGR)
		if (!qmgr_checkclick(event)) return 0;
	if (visible & VISIBLE_QLIST)
		if (!qlist_checkclick(event)) return 0;
	if (visible & VISIBLE_SITE)
		if (!site_checkclick(event)) return 0;

	return menu_checkclick(event);
}



//
// Connection to FXP.One went well
//
void main_connected(void)
{

	// Automatically, pop up the sitemgr
	visible |= VISIBLE_SITEMGR;
	visible_draw();


}


//
// A new SID was established
//
void main_site_connected(int side)
{
	char *dir;

	// Changes from "Not connected" to "please wait"
	display_set_files(side, NULL);


	// Issue PWD to any site connected. The PWD reply
	// automatically issues DIRLIST.
	dir = engine_get_startdir(side);

	if (dir)
		engine_cwd(side, dir);
	else
		engine_pwd(side);

	engine_set_startdir(side, NULL);
}


void main_go(void)
{
	if (engine_go()) {
		botbar_print("Processing Queue...");
		visible &= ~VISIBLE_DISPLAY;
		visible |= VISIBLE_QUEUEMGR;
		visible_draw();
	}
}





static void about(void)
{
	char *mesg[5];
	
	mesg[0] = "<C></29></B></U>About this program<!U><!B><!29>";
	mesg[1] = "<C></29></B><!B><!29>";
	mesg[2] = "<C></29></B>Welcome to the FXP.One ncurses client.<!B><!29>";
	mesg[3] = "<C></29></B>(c) 2005 Jörgen Lundman<!B><!29>";

	popupLabelAttrib(cdkscreen, mesg, 4, COLOR_PAIR(29));

}



static int menu_callback(EObjectType cdktype, void *object, 
						 void *clientData, chtype input)
{
	CDKMENU *menu = (CDKMENU *)object;
	int mp, sp, m;
	MEVENT event;

	getCDKMenuCurrentItem(menu, &mp, &sp);

	if (input == KEY_MOUSE) 
		if (getmouse(&event) == OK) 
			if(event.bstate & BUTTON1_CLICKED) {
				for (m = 0; m < NUM_MENUS; m++) {
					if (!menu->titleWin[m] || !menu->pullWin[m]) continue;
					if (wenclose(menu->titleWin[m], event.y, event.x)) {
						ungetch(KEY_ESC);
						return 0;
					}
					if (wmouse_trafo(menu->pullWin[m], &event.y, &event.x, FALSE) &&
						(event.y > 0) && 
						(event.y < submenusize[m])) {
						setCDKMenuCurrentItem(menu, m, event.y - 1);
						ungetch(KEY_ENTER);
						return 0;
					}
				}
			}
	
	if (input == KEY_ESC) {
		// I want it so when you open the menu, it always starts at the
		// first menu item. So you can get use to, esc / up / return to
		// always be the same choice.
		int title, sub;

		getCDKMenuCurrentItem(menu, &title, &sub);
		setCDKMenuCurrentItem(menu, title, 0);
		return 1;
	}


	if (input == KEY_ENTER) {
		switch(menu->currentTitle * 100 + menu->currentSubtitle) {

			// LEFT MENU
		case 000: // Refresh
			display_refresh(LEFT_SID);
			break;
		case 001: // Queue
			display_queue_selected(LEFT_SID);
			break;
		case 002: // Delete
			site_dele(LEFT_SID);
			break;
		case 003: // Rename
			site_rename(LEFT_SID);
			break;
		case 004: // MKD
			site_mkd(LEFT_SID);
			break;
		case 005: // Compare
			engine_compare();
			break;
		case 006: // Site
			site_cmd(LEFT_SID);
			break;

			// MAIN MENU
		case 100: // Connect
			visible |= VISIBLE_CONNECT;
			visible &= ~(VISIBLE_SITEMGR|VISIBLE_DISPLAY);
			visible_draw();
			break;
		case 101: // SiteMgr
			visible = VISIBLE_SITEMGR;
			//visible &= ~(VISIBLE_CONNECT|VISIBLE_DISPLAY);
			visible_draw();
			break;
		case 102: // Sites Display
			visible = VISIBLE_DISPLAY;
			visible_draw();
			//display_refresh(LEFT_SID);
			//display_refresh(RIGHT_SID);
			break;
		case 103: // QueueManager
			visible = VISIBLE_QUEUEMGR;
			visible_draw();
			break;
		case 104: // QueueList
			visible = VISIBLE_QLIST;
			visible_draw();
			break;
		case 105: // Go
			main_go();
			break;
		case 106: // Spacer
			about();
			break;
		case 107: // ToggleMouse
			mouse_set ^= 1;
			mousemask( mouse_set ? MOUSE_MASK : 0,
					   NULL );
			break;
		case 108: // About
			about();
			break;
		case 109: // Exit
			exitOKCDKScreen(cdkscreen);
			break;

			// RIGHT MENU
		case 200: // Refresh
			display_refresh(RIGHT_SID);
			break;
		case 201: // Queue
			display_queue_selected(RIGHT_SID);
			break;
		case 202: // Delete
			site_dele(RIGHT_SID);
			break;
		case 203: // Rename
			site_rename(RIGHT_SID);
			break;
		case 204: // MKD
			site_mkd(RIGHT_SID);
			break;
		case 205: // Compare
			engine_compare();
			break;
		case 206: // Site
			site_cmd(RIGHT_SID);
			break;




		} // switch

	} // ENTER


	//fprintf(d, "menu %02x\n", input);
	return 1;
}








int window_init(void)
{
	char *topmsg[1];
	//char *menulist[1][3];
	char buffer[1024];
	int bars, y, x;
	int menuloc[3];


	/* Initialize the Cdk screen.   */
	screen = initscr();
	if (!screen) return 1;

	cdkscreen = initCDKScreen (screen);
	if (!cdkscreen) return 2;

	initCDKColor();

	mousemask(MOUSE_MASK, NULL);
	mouse_set = 1;



	getmaxyx(screen, y, x);

	bars = x / 2 - 21;

	//snprintf(buffer, sizeof(buffer), "<L></5></B> . Sitename  %*.*s<#VL> Site Manager <#VL>%*.*s  Sitename . <!5><!B>",
	snprintf(buffer, sizeof(buffer), "<L></5></B>%*.*s<!5><!B>",
			 x, x, " ");

	topmsg[0] = buffer;

	topbar = newCDKLabel(cdkscreen, CENTER, TOP, topmsg, 1, 0, 0);


	snprintf(buffer, sizeof(buffer), "<L></5></B>%*.*s<!5><!B>",
			 x, x, " ");
	
	topmsg[0] = buffer;

	botbar = newCDKLabel(cdkscreen, LEFT, BOTTOM, topmsg, 1, 0, 0);


	menulist[0][0] = "</5></B>L<!B>eft<!5>";

	menulist[1][0] = "</5> </B>M<!B>enu<!5>";

	menulist[2][0] = "</5></B>R<!B>ight<!5>";

	// LEFT MENU
	{ // just for indent
		menulist[0][1] = "</5></B>R<!B>efresh<!5>";
		menulist[0][2] = "</5></B>Q<!B>ueue<!5>";
		menulist[0][3] = "</5></B>D<!B>elete<!5>";
		menulist[0][4] = "</5></B>R<!B>ename<!5>";
		menulist[0][5] = "</5></B>N<!B>ew Dir<!5>";
		menulist[0][6] = "</5></B>C<!B>ompare<!5>";
		menulist[0][7] = "</5>S</B>i<!B>tecmd<!5>";
		submenusize[0] = 8;
		menuloc[0]     = LEFT;
	}

	// MENU MENU
	{   // Connect, Sitemanager, sitesDisplay, Queuemgr, queueList,
		// Go, Togglemse, About, Exit
		menulist[1][1] = "</5></B>C<!B>onnect <!5>";
		menulist[1][2] = "</5></B>S<!B>ite Manager<!5>";
		menulist[1][3] = "</5>Sites </B>D<!B>isplay<!5>";
		menulist[1][4] = "</5></B>Q<!B>ueue Manager<!5>";
		menulist[1][5] = "</5>Queue </B>L<!B>ist<!5>";
		menulist[1][6] = "</5></B>^G<!B>o<!5>";
		menulist[1][7] = "</5>-----------<!5>";
		menulist[1][8] = "</5></B>T<!B>oggle Mouse<!5>";
		menulist[1][9] = "</5></B>A<!B>bout<!5>";
		menulist[1][10]= "</5></B>E<!B>xit<!5>";
		submenusize[1] = 11;
		menuloc[1]     = LEFT; // We want center, but it bugs.
	}
	// RIGHT MENU
	{   // Refresh, Queue, Delete, rEname, Newdir, Compare,
		// Sitecmd, 
		menulist[2][1] = "</5></B>R<!B>efresh<!5>";
		menulist[2][2] = "</5></B>Q<!B>ueue<!5>";
		menulist[2][3] = "</5></B>D<!B>elete<!5>";
		menulist[2][4] = "</5>R</B>e<!B>name<!5>";
		menulist[2][5] = "</5></B>N<!B>ew Dir<!5>";
		menulist[2][6] = "</5></B>C<!B>ompare<!5>";
		menulist[2][7] = "</5>S</B>i<!B>tecmd<!5>";
		submenusize[2] = 8;
		menuloc[2]     = RIGHT;
	}
	

	menu = newCDKMenu (cdkscreen, menulist, NUM_MENUS, submenusize, menuloc,
					   TOP, A_UNDERLINE, A_BOLD);

	if (!menu) return 1;


	// Move is relative - We move it as we want it centered.
	// There is no CENTER support.
	moveCursesWindow(menu->titleWin[1], COLS/2-(8), 0);
	moveCursesWindow(menu->pullWin[1], COLS/2-(8), 0);
	refreshCDKWindow(menu->titleWin[1]);
	refreshCDKWindow(menu->pullWin[1]);


	setCDKMenuPreProcess(menu, menu_callback, NULL);

	botbar_print("Pick a method to connect to FXP.One");

	return 0;
}



void welcome_dialog(void)
{
	refreshCDKScreen(cdkscreen);

	//	waitCDKLabel(topbar, ' ');
#if 0
	{
		WINDOW *test;
		CDKLABEL *test2;
		unsigned char utf8[] = 
			{0xE6,0x97,0xA5,0xE6,0x9C,0xAC,0xE8,0xAA,
			 0x9E,0x2E,0x74,0x78,0x74,0x00};
		char *mesg[2] = { "Test is CDK test", utf8 };


		test = newwin(10, 80, 10, 0);

		if (!test) return;

		wprintw(test, "Testing it '%s'...\n", utf8);

		wrefresh(test);


		test2 = newCDKLabel(cdkscreen, CENTER, CENTER, mesg, 2, FALSE, FALSE);
		drawCDKLabel(test2, FALSE);


		getch();

		delwin(test);

	}
#endif


}


void window_free(void)
{

	mousemask(0, NULL);

	if (topbar) {
		destroyCDKLabel(topbar);
		topbar = NULL;
	}
	if (botbar) {
		eraseCDKLabel(botbar);
		destroyCDKLabel(botbar);
		botbar = NULL;
	}
	if (menu) {
		destroyCDKMenu(menu);
		menu = NULL;
	}
	destroyCDKScreen(cdkscreen);
	cdkscreen = NULL;
	delwin(screen);
	screen = NULL;
	endCDK();
}













//
// Here, we work out if we should create and draw new 
// parts of the screen by remembering the last state.
void visible_draw(void)
{
	static unsigned int last_visible;

	if (!(visible & VISIBLE_CONNECT) &&
		(last_visible & VISIBLE_CONNECT)) {
		undraw_connect();
	}

	if (!(visible & VISIBLE_SITEMGR) &&
		(last_visible & VISIBLE_SITEMGR)) {
		sitemgr_undraw();
		// Make sure we have a valid focus until the next screen comes up
		my_setCDKFocusCurrent(cdkscreen, ObjPtr(menu));
	}

	if (!(visible & VISIBLE_EDITSITE) &&
		(last_visible & VISIBLE_EDITSITE)) {
		sitemgr_undraw_edit();
		// Make sure we have a valid focus until the next screen comes up
		my_setCDKFocusCurrent(cdkscreen, ObjPtr(menu));
	}

	if (!(visible & VISIBLE_DISPLAY) &&
		(last_visible & VISIBLE_DISPLAY)) {
		display_undraw();
		// Make sure we have a valid focus until the next screen comes up
		my_setCDKFocusCurrent(cdkscreen, ObjPtr(menu));
	}

	if (!(visible & VISIBLE_QUEUEMGR) &&
		(last_visible & VISIBLE_QUEUEMGR)) {
		qmgr_undraw();
	}

	if (!(visible & VISIBLE_QLIST) &&
		(last_visible & VISIBLE_QLIST)) {
		qlist_undraw();
	}

	if (!(visible & VISIBLE_SITE) &&
		(last_visible & VISIBLE_SITE)) {
		site_undraw();
	}





	if ((visible & VISIBLE_CONNECT) &&
		!(last_visible & VISIBLE_CONNECT)) {
		draw_connect();
	}

	if ((visible & VISIBLE_SITEMGR) &&
		!(last_visible & VISIBLE_SITEMGR)) {
		sitemgr_draw();
	}
	
	if ((visible & VISIBLE_EDITSITE) &&
		!(last_visible & VISIBLE_EDITSITE)) {
		sitemgr_draw_edit();
	}

	if ((visible & VISIBLE_DISPLAY) &&
		!(last_visible & VISIBLE_DISPLAY)) {
		display_draw();
	}
	
	if ((visible & VISIBLE_QUEUEMGR) &&
		!(last_visible & VISIBLE_QUEUEMGR)) {
		qmgr_draw();
	}

	if ((visible & VISIBLE_QLIST) &&
		!(last_visible & VISIBLE_QLIST)) {
		qlist_draw();
	}

	if ((visible & VISIBLE_SITE) &&
		!(last_visible & VISIBLE_SITE)) {
		site_draw();
	}





	last_visible = visible;

}







int main(int argc, char **argv)
{
	d = fopen("out.log", "a");
	if (d) setvbuf(d, NULL, _IOLBF, 1024);


	if (engine_init())
		exit(-1);

	if (window_init())
		exit(-2);



	welcome_dialog();


	visible = VISIBLE_CONNECT;
	//visible |= VISIBLE_EDITSITE;
	//visible |= VISIBLE_DISPLAY;
	//visible |= VISIBLE_QUEUEMGR;
	//visible = VISIBLE_SITE;
	visible_draw();

	while (!do_quit) {
		
		if (my_traverseCDKScreen(cdkscreen) != -1) 
			break;

		if (visible & VISIBLE_CONNECT) 
			connect_poll();

		if (visible & (VISIBLE_SITEMGR | VISIBLE_EDITSITE))
			sitemgr_poll();

		if (visible & (VISIBLE_DISPLAY))
			display_poll();

		if (visible & (VISIBLE_QUEUEMGR))
			qmgr_poll();

		if (visible & (VISIBLE_QLIST))
			qlist_poll();

		if (visible & (VISIBLE_SITE))
			site_poll();

		engine_poll();

	}


	// Undraw everything
	visible = 0;
	visible_draw();



	window_free();

	engine_free();



	if (d)
		fclose(d);
	exit (0);
}







