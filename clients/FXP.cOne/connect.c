#include <stdio.h>

#include <ncurses.h>
#include <cdk/cdk.h>

#include "lion.h"

#include "main.h"
#include "connect.h"
#include "engine.h"
#include "traverse.h"


static CDKRADIO     *cradio        = NULL;
static CDKLABEL     *cbox          = NULL;
static CDKLABEL     *labels        = NULL;
static CDKENTRY     *entry_key     = NULL;
static CDKENTRY     *entry_host    = NULL;
static CDKENTRY     *entry_port    = NULL;
static CDKENTRY     *entry_user    = NULL;
static CDKENTRY     *entry_pass    = NULL;
static CDKBUTTONBOX *okbutton      = NULL;
static CDKBUTTONBOX *quitbutton    = NULL;

extern FILE *d;
static int begx  = 0,  begy   = 0;
static int width = 60, height = 20;
static int connect_ok = 0;



static int radio_callback(EObjectType cdktype, void *object, 
						  void *clientData, chtype input)
{
	//CDKRADIO *lradio = (CDKRADIO *) object;
	MEVENT event;

	if (input == KEY_MOUSE) 
		if (getmouse(&event) == OK) 
			return check_all_mouse(&event);
	
	
	if (input == ' ')
		connect_draw_inputs();

	return 1;
}




static int quitbutton_callback(EObjectType cdktype, void *object, 
							   void *clientData, chtype input)
{
	CDKBUTTONBOX *button = (CDKBUTTONBOX *) object;
	MEVENT event;
	
	if (input == KEY_MOUSE) 
		if (getmouse(&event) == OK) {
			if(event.bstate & BUTTON1_CLICKED) 
				if (wenclose(button->win, event.y, event.x)) {
					exitCancelCDKScreen(cdkscreen);
					return 1;
				}
			return check_all_mouse(&event);
		} // get event


	if (input == KEY_ENTER)
		exitCancelCDKScreen(cdkscreen);


	return 1;
}





static int okbutton_callback(EObjectType cdktype, void *object, 
							 void *clientData, chtype input)
{
	CDKBUTTONBOX *button = (CDKBUTTONBOX *) object;
	MEVENT event;
	
	if (input == KEY_MOUSE) 
		if (getmouse(&event) == OK) {
			if(event.bstate & BUTTON1_CLICKED) 
				if (wenclose(button->win, event.y, event.x)) {
					connect_ok = 1;
					return 1;
				}
			return check_all_mouse(&event);
		} // get event


	if (input == KEY_ENTER)
		connect_ok = 1;


	return 1;
}





static int entrykey_callback(EObjectType cdktype, void *object, 
							 void *clientData, chtype input)
{
	CDKBUTTONBOX *button = (CDKBUTTONBOX *) object;
	MEVENT event;
	
	if (input == KEY_MOUSE) 
		if (getmouse(&event) == OK) {
			if(event.bstate & BUTTON1_CLICKED) 
				if (wenclose(button->win, event.y, event.x)) {
					my_setCDKFocusCurrent(cdkscreen, ObjPtr(button));
					return 1;
				}
			return check_all_mouse(&event);
		} // get event


	if (input == KEY_ENTER)
		connect_ok = 1;


	return 1;
}



static int entryhost_callback(EObjectType cdktype, void *object, 
							  void *clientData, chtype input)
{
	CDKBUTTONBOX *button = (CDKBUTTONBOX *) object;
	MEVENT event;
	
	if (input == KEY_MOUSE) 
		if (getmouse(&event) == OK) {
			if(event.bstate & BUTTON1_CLICKED) 
				if (wenclose(button->win, event.y, event.x)) {
					my_setCDKFocusCurrent(cdkscreen, ObjPtr(button));
					return 1;
				}
			return check_all_mouse(&event);
		} // get event


	if (input == KEY_ENTER)
		connect_ok = 1;


	return 1;
}




static int entryport_callback(EObjectType cdktype, void *object, 
							 void *clientData, chtype input)
{
	CDKBUTTONBOX *button = (CDKBUTTONBOX *) object;
	MEVENT event;
	
	if (input == KEY_MOUSE) 
		if (getmouse(&event) == OK) {
			if(event.bstate & BUTTON1_CLICKED) 
				if (wenclose(button->win, event.y, event.x)) {
					my_setCDKFocusCurrent(cdkscreen, ObjPtr(button));
					return 1;
				}
			return check_all_mouse(&event);
		} // get event


	if (input == KEY_ENTER)
		connect_ok = 1;


	return 1;
}




static int entryuser_callback(EObjectType cdktype, void *object, 
							  void *clientData, chtype input)
{
	CDKBUTTONBOX *button = (CDKBUTTONBOX *) object;
	MEVENT event;
	
	if (input == KEY_MOUSE) 
		if (getmouse(&event) == OK) {
			if(event.bstate & BUTTON1_CLICKED) 
				if (wenclose(button->win, event.y, event.x)) {
					my_setCDKFocusCurrent(cdkscreen, ObjPtr(button));
					return 1;
				}
			return check_all_mouse(&event);
		} // get event


	if (input == KEY_ENTER)
		connect_ok = 1;


	return 1;
}





static int entrypass_callback(EObjectType cdktype, void *object, 
							  void *clientData, chtype input)
{
	CDKBUTTONBOX *button = (CDKBUTTONBOX *) object;
	MEVENT event;
	
	if (input == KEY_MOUSE) 
		if (getmouse(&event) == OK) {
			if(event.bstate & BUTTON1_CLICKED) 
				if (wenclose(button->win, event.y, event.x)) {
					my_setCDKFocusCurrent(cdkscreen, ObjPtr(button));
					return 1;
				}
			return check_all_mouse(&event);
		} // get event


	if (input == KEY_ENTER)
		connect_ok = 1;


	return 1;
}





void connect_draw_inputs(void)
{
	char *sel0msg[] = { 
		"<R></33> Key:<!33>",
		"<R></33>     <!33>",
		"<R></33>     <!33>",
		"<R></33>User:<!33>",
		"<R></33>Pass:<!33>"};
	char *sel1msg[] = { 
		"<R></33>     <!33>",
		"<R></33>Host:<!33>",
		"<R></33>Port:<!33>",
		"<R></33>User:<!33>",
		"<R></33>Pass:<!33>"};
	char **choice;

	choice = sel0msg;

	// Technically we are getting currentItem here, when we
	// really want to look at selectedItem. But it's not set
	// until after this PreProcess function returns.
	switch (getCDKRadioCurrentItem(cradio)) {
	case 0:
		choice = sel0msg;
		setCDKEntryBackgroundColor(entry_key, "</0>");
		drawCDKEntry(entry_key, FALSE);
		AcceptsFocusObj(entry_key)=TRUE;

		setCDKEntryBackgroundColor(entry_host, "</33>");
		drawCDKEntry(entry_host, FALSE);
		eraseCDKEntry(entry_host);
		AcceptsFocusObj(entry_host)=FALSE;
		setCDKEntryBackgroundColor(entry_port, "</33>");
		drawCDKEntry(entry_port, FALSE);
		eraseCDKEntry(entry_port);
		AcceptsFocusObj(entry_port)=FALSE;
		break;
	case 1:
		choice = sel1msg;
		setCDKEntryBackgroundColor(entry_key, "</33>");
		drawCDKEntry(entry_key, FALSE);
		eraseCDKEntry(entry_key);
		AcceptsFocusObj(entry_key)=FALSE;

		setCDKEntryBackgroundColor(entry_host, "</0>");
		drawCDKEntry(entry_host, FALSE);
		AcceptsFocusObj(entry_host)=TRUE;
		setCDKEntryBackgroundColor(entry_port, "</0>");
		drawCDKEntry(entry_port, FALSE);
		AcceptsFocusObj(entry_port)=TRUE;
		break;
	}
	
	setCDKLabelMessage(labels, choice, 5);

}






//
// Draw the connect dialog
//
//  () Spawn new FXP.One       Key: [        ]
//  () Connect to existing    Host: [        ]  Port: [       ]
//
//     User: [        ]       Pass: [        ]        [ QUIT ]
//
void draw_connect(void)
{
	// What size should we make it?
	int i;
	char boxtop[]  = "</33><#UL><#HL(58)><#UR><!33>";
	char boxline[] = "</33><#VL>                                                          <#VL><!33>";
	char boxbot[]  = "</33><#LL><#HL(58)><#LR><!33>";
	char *boxmsg[height];
	char *items[2] = { "Spawn a new FXP.One engine", "Connect to existing FXP.One Engine"};
	char *labelmsg[] = { 
		"<R></33>     <!33>",
		"<R></33>     <!33>",
		"<R></33>     <!33>",
		"<R></33>     <!33>",
		"<R></33>     <!33>"};
	char *okmsg[]   = {"</5>  OK  <!5>"};
	char *quitmsg[] = {"</5> Quit <!5>"};

	for (i = 1; i < height-1; i++)
		boxmsg[i] = boxline;
	boxmsg[0] = boxtop;
	boxmsg[height-1] = boxbot;

	begx = (COLS>>1)-(width>>1);
	begy = (LINES>>1)-(height>>1);


	cbox = newCDKLabel(cdkscreen, begx, begy, boxmsg, height, FALSE, FALSE);
	if (!cbox) goto fail;



	// Create items.
	cradio = newCDKRadio(cdkscreen,
						 begx + 3,
						 begy + 3,
						 NONE,
						 4,
						 width-10,
						 NULL,
						 items,
						 2,
						 '*'|A_BOLD,
						 0,  // defItem is ignored.
						 A_REVERSE,
						 FALSE,
						 FALSE);
	if (!cradio) goto fail;

	//As defItem is ignored in newCDKRadio, we need to fudge it here
	setCDKRadioCurrentItem(cradio, 1);
	cradio->selectedItem = 1;

	setCDKRadioBackgroundColor(cradio, "</33>");

	setCDKRadioPreProcess(cradio, radio_callback, NULL);


	labels = newCDKLabel(cdkscreen, begx+2, begy+7, 
						 labelmsg,
						 5, 0, 0);
	if (!labels) goto fail;


	entry_key = newCDKEntry(cdkscreen, 
							begx + 8,
							begy + 7,
							NULL,
							NULL,
							A_NORMAL,
							' ',
							vHMIXED,
							42,   // width
							0,    // min
							256,  // max
							FALSE,
							FALSE);
	if (!entry_key) goto fail;


	setCDKEntryValue(entry_key, "secret");
	setCDKEntryHiddenChar(entry_key, '*');


	entry_host = newCDKEntry(cdkscreen, 
							 begx + 8,
							 begy + 8,
							 NULL,
							 NULL,
							 A_NORMAL,
							 ' ',
							 vMIXED,
							 42,   // width
							 0,    // min
							 256,  // max
							 FALSE,
							 FALSE);
	if (!entry_host) goto fail;

	setCDKEntryValue(entry_host, "localhost");



	entry_port = newCDKEntry(cdkscreen, 
							 begx + 8,
							 begy + 9,
							 NULL,
							 NULL,
							 A_NORMAL,
							 ' ',
							 vINT,
							 42,   // width
							 0,    // min
							 256,  // max
							 FALSE,
							 FALSE);
	if (!entry_host) goto fail;
	
	setCDKEntryValue(entry_port, "8885");



	entry_user = newCDKEntry(cdkscreen, 
							 begx + 8,
							 begy + 10,
							 NULL,
							 NULL,
							 A_NORMAL,
							 ' ',
							 vMIXED,
							 42,   // width
							 0,    // min
							 256,  // max
							 FALSE,
							 FALSE);
	if (!entry_user) goto fail;

	setCDKEntryValue(entry_user, "admin");


	entry_pass = newCDKEntry(cdkscreen, 
							 begx + 8,
							 begy + 11,
							 NULL,
							 NULL,
							 A_NORMAL,
							 ' ',
							 vHMIXED,
							 42,   // width
							 0,    // min
							 256,  // max
							 FALSE,
							 FALSE);
	if (!entry_pass) goto fail;
	
	setCDKEntryValue(entry_pass, "");
	setCDKEntryHiddenChar(entry_pass, '*');

	my_setCDKFocusCurrent(cdkscreen, ObjPtr(entry_pass));

	okbutton = newCDKButtonbox(cdkscreen,
							   begx + 8,
							   begy + 15,
							   1,
							   10,
							   NULL,
							   1,
							   1,
							   okmsg,
							   1,
							   A_BOLD,
							   TRUE,
							   FALSE);
	if (!okbutton) goto fail;

	setCDKButtonboxBackgroundColor(okbutton, "</33>");



	quitbutton = newCDKButtonbox(cdkscreen,
								 begx + 39,
								 begy + 15,
								 1,
								 10,
								 NULL,
								 1,
								 1,
								 quitmsg,
								 1,
								 A_BOLD,
								 TRUE,
								 FALSE);
	if (!quitbutton) goto fail;
	
	setCDKButtonboxBackgroundColor(quitbutton, "</33>");

	setCDKButtonboxPreProcess(quitbutton, quitbutton_callback, NULL);
	setCDKButtonboxPreProcess(okbutton, okbutton_callback, NULL);
	setCDKEntryPreProcess(entry_key,  entrykey_callback, NULL);
	setCDKEntryPreProcess(entry_host, entryhost_callback, NULL);
	setCDKEntryPreProcess(entry_port, entryport_callback, NULL);
	setCDKEntryPreProcess(entry_user, entryuser_callback, NULL);
	setCDKEntryPreProcess(entry_pass, entrypass_callback, NULL);


	refreshCDKScreen(cdkscreen);

	connect_draw_inputs();	

	move(begy + 11, begx + 8);
	refresh();

	return;

 fail:
	undraw_connect();
	return;

}






void undraw_connect(void)
{

	if (entry_key) {
		// Setting this leaves dirt on screen, so we need to manually
		// force it back
		setCDKEntryBackgroundColor(entry_key, "</0>");
		drawCDKEntry(entry_key, FALSE);
		destroyCDKEntry(entry_key);
		entry_key = NULL;
	}

	if (entry_host) {
		setCDKEntryBackgroundColor(entry_host, "</0>");
		drawCDKEntry(entry_host, FALSE);
		destroyCDKEntry(entry_host);
		entry_host = NULL;
	}
	if (entry_port) {
		setCDKEntryBackgroundColor(entry_port, "</0>");
		drawCDKEntry(entry_port, FALSE);
		destroyCDKEntry(entry_port);
		entry_port = NULL;
	}
	if (entry_user) {
		destroyCDKEntry(entry_user);
		entry_user = NULL;
	}
	if (entry_pass) {
		destroyCDKEntry(entry_pass);
		entry_pass = NULL;
	}
	if (labels) {
		destroyCDKLabel(labels);
		labels = NULL;
	}
	if (okbutton) {
		destroyCDKButtonbox(okbutton);
		okbutton = NULL;
	}
	if (quitbutton) {
		destroyCDKButtonbox(quitbutton);
		quitbutton = NULL;
	}

	if (cbox) {
		destroyCDKLabel(cbox);
		cbox = NULL;
	}

	if (cradio) {
		// Setting background leaves dirt on screen
		setCDKRadioBackgroundColor(cradio, "</0>");
		destroyCDKRadio(cradio);
		cradio = NULL;
	}

}



#define INSIDE_BOUNDINGBOX(PX,PY,X1,Y1,X2,Y2) ((PX>=X1)&&(PX<=X2)&&(PY>=Y1)&&(PY<=Y2))




int connect_checkclick(MEVENT *event)
{
	// Compute if this mouse event happened inside the connect dialog.
	// begx      <->     begx+width
	// begy
	// begy+height

	if(event->bstate & BUTTON1_CLICKED) { 
		
		
		if (INSIDE_BOUNDINGBOX(event->x, event->y, begx, begy, begx+width, begy+height)) {
			
			if (wenclose(cradio->win, event->y, event->x)) {
				
				if (wmouse_trafo(cradio->win, &event->y, &event->x, FALSE) &&
					(event->y >= 0) && 
					(event->y < 2)) {
					setCDKRadioCurrentItem(cradio, event->y);
					my_setCDKFocusCurrent(cdkscreen, ObjPtr(cradio));
					ungetch(' ');
					return 0;
				}
			}
			
			
			
			if (wenclose(entry_key->win, event->y, event->x)) {
				
				// Only focus this item if it's active.
				if (!getCDKRadioCurrentItem(cradio)) {
					my_setCDKFocusCurrent(cdkscreen, ObjPtr(entry_key));	
					return 0;
				}
			}
			
			
			if (wenclose(entry_host->win, event->y, event->x)) {
				
				// Only focus this item if it's active.
				if (getCDKRadioCurrentItem(cradio)) {
					my_setCDKFocusCurrent(cdkscreen, ObjPtr(entry_host));	
					return 0;
				}
			}
			
			if (wenclose(entry_port->win, event->y, event->x)) {
				
				// Only focus this item if it's active.
				if (getCDKRadioCurrentItem(cradio)) {
					my_setCDKFocusCurrent(cdkscreen, ObjPtr(entry_port));	
					return 0;
				}
			}
			
			
			if (wenclose(entry_user->win, event->y, event->x)) {
				my_setCDKFocusCurrent(cdkscreen, ObjPtr(entry_user));	
				return 0;
			}
			
			if (wenclose(entry_pass->win, event->y, event->x)) {
				my_setCDKFocusCurrent(cdkscreen, ObjPtr(entry_pass));	
				return 0;
			}
			
			if (wenclose(okbutton->win, event->y, event->x)) {
				ungetch(KEY_ENTER);
				return 0;
			}
			
			if (wenclose(quitbutton->win, event->y, event->x)) {
				exitOKCDKScreen(cdkscreen);
				return 0;
			}
			
			
			return 0;

		}

	}

	return 1;
}



void connect_poll(void)
{
	char *key, *host, *port, *user, *pass;
	

	if (connect_ok) {

		connect_ok = 0;
		
		key  = getCDKEntryValue(entry_key);
		host = getCDKEntryValue(entry_host);
		port = getCDKEntryValue(entry_port);
		user = getCDKEntryValue(entry_user);
		pass = getCDKEntryValue(entry_pass);
		
		if (cradio->selectedItem)
			engine_connect(host, atoi(port), user, pass);
		else
			engine_spawn(key, user, pass);
				
		
		visible &= ~VISIBLE_CONNECT;
		visible_draw();


	}
	
}



