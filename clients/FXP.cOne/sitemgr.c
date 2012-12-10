#include <stdio.h>
#include <cdk/cdk.h>

#include "lion.h"
#include "misc.h"

#include "traverse.h"
#include "main.h"
#include "parser.h"
#include "sitemgr.h"
#include "engine.h"




static site_t       *sites             = NULL;
static unsigned int  num_sites         = 0;

static int           connect_ok        = 0;

// 1 Edit, 2 Save, 3 Cancel, 4 Del, 5 Copy
static int           connect_edit      = 0;


static CDKSCROLL        *sm_scroll     = NULL;
static CDKLABEL         *labelleft     = NULL;
static CDKLABEL         *labelright    = NULL;
static CDKBUTTONBOX     *connbutton    = NULL;
static CDKBUTTONBOX     *editbutton    = NULL;
static CDKBUTTONBOX     *addbutton     = NULL;
static CDKBUTTONBOX     *delbutton     = NULL;
static CDKBUTTONBOX     *copybutton    = NULL;
static CDKMATRIX        *edit_matrix   = NULL;
static CDKBUTTONBOX     *savebutton    = NULL;
static CDKBUTTONBOX     *cancelbutton  = NULL;
static CDKBUTTONBOX     *addkpbutton   = NULL;
static CDKBUTTONBOX     *delkpbutton   = NULL;
static CDKLABEL         *nfo_msg       = NULL;

static unsigned int      matrix_numkp  = 0;

static site_t *left_site  = NULL;
static site_t *right_site = NULL;




static int scroll_callback(EObjectType cdktype, void *object, 
						   void *clientData, chtype input)
{
	CDKSCROLL *lscroll = (CDKSCROLL *) object;
	MEVENT event;
	int i;
	site_t *site;

	if (input == KEY_MOUSE) 
		if (getmouse(&event) == OK) {
			check_all_mouse(&event);
		} // get event

	if (input == KEY_LEFT) {
		// Get site in question, then hilight it.
		i = getCDKScrollCurrent(lscroll);
		site = sitemgr_getby_index(i);
		sitemgr_set_left(site);
		return 1;
	}

	if (input == KEY_RIGHT) {
		// Get site in question, then hilight it.
		i = getCDKScrollCurrent(lscroll);
		site = sitemgr_getby_index(i);
		sitemgr_set_right(site);
		return 1;
	}

	if (input == KEY_ENTER) {
		connect_ok = 1;
	}

	return 1;
}



static int conn_callback(EObjectType cdktype, void *object, 
						   void *clientData, chtype input)
{
	MEVENT event;

	if (input == KEY_MOUSE) 
		if (getmouse(&event) == OK) {
			return check_all_mouse(&event);
		} // get event
	
	if (input == KEY_ENTER) {
		if (left_site || right_site)
			connect_ok = 1;
	}

	return 1;
}




static int edit_callback(EObjectType cdktype, void *object, 
						   void *clientData, chtype input)
{
	MEVENT event;

	if (input == KEY_MOUSE) 
		if (getmouse(&event) == OK) {
			return check_all_mouse(&event);
		} // get event
	
	if (input == KEY_ENTER) {
		if (left_site)
			connect_edit = 1;
	}

	return 1;
}



static int add_callback(EObjectType cdktype, void *object, 
						void *clientData, chtype input)
{
	MEVENT event;

	if (input == KEY_MOUSE) 
		if (getmouse(&event) == OK) {
			return check_all_mouse(&event);
		} // get event
	
	if (input == KEY_ENTER) {
		// Add is edit of an empty site.
		left_site = NULL;
		connect_edit = 1;
	}

	return 1;
}



static int del_callback(EObjectType cdktype, void *object, 
						void *clientData, chtype input)
{
	MEVENT event;
	int i;
	char buff[1024];
	char *suremsg[1];
	char *sure_butts[2] = { "Yes", "No" };

	if (input == KEY_MOUSE) 
		if (getmouse(&event) == OK) {
			return check_all_mouse(&event);
		} // get event
	
	if (input == KEY_ENTER) {
		// Are you sure dialog.

		if (left_site && left_site->name) {
			
			snprintf(buff, sizeof(buff), "Are you sure you want to delete site '%s' (id %d) ?",
					 left_site->name,
					 left_site->siteid);
			
			suremsg[0] = buff;
			
			i = popupDialog(cdkscreen, suremsg, 1, sure_butts, 2);
			
			if (i == 0) // YES
				connect_edit = 4; // delete
			else
				connect_edit = 3; // cancel
			return 1;

		}

		connect_edit = 3; // cancel
	}

	return 1;
}




static int copy_callback(EObjectType cdktype, void *object, 
						 void *clientData, chtype input)
{
	MEVENT event;

	if (input == KEY_MOUSE) 
		if (getmouse(&event) == OK) {
			return check_all_mouse(&event);
		} // get event
	
	if (input == KEY_ENTER) {
		// Add is edit of an empty site.
		if (left_site)
			connect_edit = 5;
	}

	return 1;
}




void sitemgr_set_left(site_t *site)
{
	char buffer[1024];
	char *leftmsg[1];

	snprintf(buffer, sizeof(buffer), "Connect left : %-20.20s",
			 site && site->name ? site->name : "x");

	leftmsg[0] = buffer;
	
	if (labelleft)
		setCDKLabelMessage(labelleft,
							leftmsg,
							1);
	left_site = site;
	
}



void sitemgr_set_right(site_t *site)
{

	char buffer[1024];
	char *rightmsg[1];

	snprintf(buffer, sizeof(buffer), "Connect right: %-20.20s",
			 site && site->name ? site->name : "x");

	rightmsg[0] = buffer;
	
	if (labelright)
		setCDKLabelMessage(labelright,
							rightmsg,
							1);
	right_site = site;

}




site_t *sitemgr_getby_index(unsigned int n)
{
	site_t *result;

	if (n > num_sites) return NULL;

	result = sites;

	while (n > 0) {
		if (!result) return NULL;
		result = result->next;
		n--;
	}

	return result;
}





site_t *sitemgr_newnode(void)
{
	site_t *result;


	result = calloc(sizeof(*result), 1);

	if (!result) return NULL;

	result->siteid       = -1;

	// Set the defaults for sites.
	result->port         = 21;
	result->passive      = 2;  // Auto
	result->fxp_passive  = 2;  // Auto
	result->control_TLS  = 2;  // Auto
	result->data_TLS     = 2;  // Auto
	result->desired_type = 2;  // Auto
	result->resume       = 2;  // Auto
	result->resume_last  = 2;  // Auto
	result->pret         = 2;  // Auto
	result->fskipempty   = 2;  // Auto
	result->dskipempty   = 2;  // Auto

	return result;
}


void sitemgr_freenode(site_t *node)
{
	int i;

	for (i = 0; i < node->items; i++) {
		SAFE_FREE(node->keys[i]);
		SAFE_FREE(node->values[i]);
	}
	SAFE_FREE(node->keys);
	SAFE_FREE(node->values);
	node->items = 0;

	SAFE_FREE(node->name);
	SAFE_FREE(node->host);
	SAFE_FREE(node->iface);
	SAFE_FREE(node->user);
	SAFE_FREE(node->pass);
	SAFE_FREE(node->fskiplist);
	SAFE_FREE(node->dskiplist);
	SAFE_FREE(node->fpasslist);
	SAFE_FREE(node->dpasslist);
	SAFE_FREE(node->fmovefirst);
	SAFE_FREE(node->dmovefirst);

	SAFE_FREE(node);

}



void sitemgr_freeallnodes(void)
{
	site_t *node, *next;

	for (node = sites; node; node = next) {
		next = node->next;
		sitemgr_freenode(node);
	}

	sites = NULL;
	num_sites = 0;

	left_site = NULL;
	right_site = NULL;

}





int str2yna(char *v)
{
	if (!v || !*v) return 2; // Auto is default
	if (!mystrccmp("NO", v) ||
		(*v == '0'))
		return 0;
	if (!mystrccmp("YES", v) ||
		(atoi(v) == 1))
		return 1;

	// Default is Auto
	return 2;
}


char *yna2str(int value)
{
	switch(value) {
	case 0:
		return "NO";
	case 1:
		return "YES";
	default:
		return "AUTO";
	}
	return "AUTO";
}





void sitemgr_site(char **keys, char **values, int items)
{
	char *key, **tmp;
	site_t *node, *runner, *prev;
	int siteid = -1;  // 0 is a valid ID.
	int i;

	if (!items) {
		botbar_print("");
		sitemgr_draw_4real();
		return;
	}

	key = parser_findkey_once(keys, values, items, "SITEID");
	if (!key) return;
	siteid = atoi(key);

	node = sitemgr_newnode();
	if (!node) return;
	
	key = parser_findkey_once(keys, values, items, "NAME");
	SAFE_COPY(node->name, key);
	key = parser_findkey_once(keys, values, items, "HOST");
	SAFE_COPY(node->host, key);
	key = parser_findkey_once(keys, values, items, "PORT");
	if (key) node->port = atoi(key);
	key = parser_findkey_once(keys, values, items, "IFACE");
	SAFE_COPY(node->iface, key);
	key = parser_findkey_once(keys, values, items, "IPORT");
	if (key) node->iport = atoi(key);
	key = parser_findkey_once(keys, values, items, "USER");
	SAFE_COPY(node->user, key);
	key = parser_findkey_once(keys, values, items, "PASS");
	SAFE_COPY(node->pass, key);
	key = parser_findkey_once(keys, values, items, "PASSIVE");
	node->passive = str2yna(key);
	key = parser_findkey_once(keys, values, items, "FXP_PASSIVE");
	node->fxp_passive = str2yna(key);
	key = parser_findkey_once(keys, values, items, "CONTROL_TLS");
	node->control_TLS = str2yna(key);
	key = parser_findkey_once(keys, values, items, "DATA_TLS");
	node->data_TLS = str2yna(key);
	key = parser_findkey_once(keys, values, items, "DESIRED_TYPE");
	node->desired_type = str2yna(key);
	key = parser_findkey_once(keys, values, items, "RESUME");
	node->resume = str2yna(key);
	key = parser_findkey_once(keys, values, items, "RESUME_LAST");
	node->resume_last = str2yna(key);
	key = parser_findkey_once(keys, values, items, "PRET");
	node->pret = str2yna(key);
	key = parser_findkey_once(keys, values, items, "FSKIPLIST");
	SAFE_COPY(node->fskiplist, key);
	key = parser_findkey_once(keys, values, items, "DSKIPLIST");
	SAFE_COPY(node->dskiplist, key);
	key = parser_findkey_once(keys, values, items, "FPASSLIST");
	SAFE_COPY(node->fpasslist, key);
	key = parser_findkey_once(keys, values, items, "DPASSLIST");
	SAFE_COPY(node->dpasslist, key);
	key = parser_findkey_once(keys, values, items, "FMOVEFIRST");
	SAFE_COPY(node->fmovefirst, key);
	key = parser_findkey_once(keys, values, items, "DMOVEFIRST");
	SAFE_COPY(node->dmovefirst, key);
	key = parser_findkey_once(keys, values, items, "FSKIPEMPTY");
	node->fskipempty = str2yna(key);
	key = parser_findkey_once(keys, values, items, "DSKIPEMPTY");
	node->dskipempty = str2yna(key);

	// Deal with client added variables. First eat those pre-defined pairs
	// so we dont process them
	//key = parser_findkey_once(keys, values, items, "SITEID");
	key = parser_findkey_once(keys, values, items, "type");
	
	// For the remaining key/pairs
	for (i = 0; i < items; i++) {

		// Still set?
		if (keys[i] && values[i]) { 

			// Increase arrays by one.
			tmp = realloc(node->keys,
						  sizeof(char *) * (node->items + 1));
			if (!tmp) continue;
			node->keys = tmp;
			tmp = realloc(node->values,
						  sizeof(char *) * (node->items + 1));
			if (!tmp) continue;
			node->values = tmp;
			node->keys  [ node->items ] = strdup(keys[i]);
			node->values[ node->items ] = strdup(values[i]);
			node->items++;

		}

	}




	node->siteid = siteid;

	num_sites++;

	// Lets insert sorted.
	for (runner = sites, prev = NULL; 
		 runner; 
		 prev = runner, runner = runner->next) {

		if (strcasecmp(runner->name, node->name) > 0) break;

	}

	if (!prev) {
		node->next = sites;
		sites = node;
	} else {
		node->next = runner;
		prev->next = node;
	}
}





void sitemgr_draw(void)
{

	botbar_print("Fetching site list...");

	// Release existing nodes
	sitemgr_freeallnodes();

	engine_sitelist(sitemgr_site);

}




void sitemgr_undraw(void)
{

	if (sm_scroll) {
		destroyCDKScroll(sm_scroll);
		sm_scroll = NULL;
	}

	if (labelleft) {
		destroyCDKLabel(labelleft);
		labelleft = NULL;
	}

	if (labelright) {
		destroyCDKLabel(labelright);
		labelright = NULL;
	}

	if (connbutton) {
		destroyCDKButtonbox(connbutton);
		connbutton = NULL;
	}

	if (editbutton) {
		destroyCDKButtonbox(editbutton);
		editbutton = NULL;
	}

	if (addbutton) {
		destroyCDKButtonbox(addbutton);
		addbutton = NULL;
	}

	if (delbutton) {
		destroyCDKButtonbox(delbutton);
		delbutton = NULL;
	}

	if (copybutton) {
		destroyCDKButtonbox(copybutton);
		copybutton = NULL;
	}

}




void sitemgr_draw_4real(void)
{
	char **thesites;
	site_t *site;
	int i, begx, begy, box_w, box_h;
	char *labelleftmsg[]  = {"Connect left :                     "};
	char *labelrightmsg[] = {"Connect right:                     "};
	char *connmsg[] = {" Connect"};
	char *editmsg[] = {" Edit"};
	char *addmsg[] = {" Add"};
	char *delmsg[] = {" Del"};
	char *copymsg[] = {" Copy"};

	// Make a box which is the size of the screen, or, num_sites
	// high.
	begx = 1;
	begy = 1;
	box_w = COLS-2;
	box_h = LINES-2;


	
	labelleft = newCDKLabel(cdkscreen, begx + 24,
							begy+1, 
							labelleftmsg,
							1,
							FALSE,
							FALSE);
	if (!labelleft) goto fail;


	labelright = newCDKLabel(cdkscreen, begx + 24,
							 begy+2, 
							 labelrightmsg,
							 1,
							 FALSE,
							 FALSE);
	if (!labelright) goto fail;


	connbutton = newCDKButtonbox(cdkscreen,
								 begx + 24,
								 begy + 4,
								 1,
								 10,
								 NULL,
								 1,
								 1,
								 connmsg,
								 1,
								 A_BOLD,
								 TRUE,
								 FALSE);
	if (!connbutton) goto fail;



	editbutton = newCDKButtonbox(cdkscreen,
								 begx + 24,
								 begy + 7,
								 1,
								 10,
								 NULL,
								 1,
								 1,
								 editmsg,
								 1,
								 A_BOLD,
								 TRUE,
								 FALSE);
	if (!editbutton) goto fail;



	addbutton = newCDKButtonbox(cdkscreen,
								begx + 24,
								begy + 10,
								1,
								10,
								NULL,
								1,
								1,
								addmsg,
								1,
								A_BOLD,
								TRUE,
								FALSE);
	if (!addbutton) goto fail;



	delbutton = newCDKButtonbox(cdkscreen,
								begx + 24,
								begy + 13,
								1,
								10,
								NULL,
								1,
								1,
								delmsg,
								1,
								A_BOLD,
								TRUE,
								FALSE);
	if (!delbutton) goto fail;


	copybutton = newCDKButtonbox(cdkscreen,
								 begx + 24,
								 begy + 16,
								 1,
								 10,
								 NULL,
								 1,
								 1,
								 copymsg,
								 1,
								 A_BOLD,
								 TRUE,
								 FALSE);
	if (!copybutton) goto fail;





	if (num_sites) {

		if (num_sites < (box_h - 1))
			box_h = num_sites+begy+1;

		thesites = malloc(sizeof(char *) * num_sites);
		if (!thesites) goto fail;
		
		
		for (site = sites, i = 0; 
			 site; 
			 site = site->next, i++) {
			thesites[i] = site->name;
		}
		
		sm_scroll = newCDKScroll(cdkscreen,
								 begx,
								 begy,
								 num_sites > box_h ? LEFT : NONE,
								 box_h,
								 20,
								 NULL,
								 thesites,
								 num_sites,
								 FALSE,
								 A_BOLD,
								 TRUE,
								 FALSE);
		if (!sm_scroll) goto fail;
		
		
		SAFE_FREE(thesites);

		setCDKScrollPreProcess(sm_scroll,  scroll_callback, NULL);

	} // numsites




	setCDKScrollPreProcess(connbutton, conn_callback, NULL);
	setCDKScrollPreProcess(editbutton, edit_callback, NULL);
	setCDKScrollPreProcess(addbutton,  add_callback,  NULL);
	setCDKScrollPreProcess(delbutton,  del_callback,  NULL);
	setCDKScrollPreProcess(copybutton, copy_callback, NULL);

	botbar_print("Edit/Del/Copy operations are performed on Left selected site");

	if (sm_scroll)
		my_setCDKFocusCurrent(cdkscreen, ObjPtr(sm_scroll));
	else
		my_setCDKFocusCurrent(cdkscreen, ObjPtr(addbutton));

	refreshCDKScreen(cdkscreen);

	return;

 fail:
	sitemgr_undraw();
	return;
}




int sitemgr_checkclick(MEVENT *event)
{
	static int left = 0;

	// If sitemgr is up, check clicks.
	if (visible & VISIBLE_SITEMGR) {

		if(event->bstate & BUTTON1_CLICKED) {
			
			// Clicked in itemlist?
			if (wenclose(sm_scroll->listWin, event->y, event->x)) {
				
				// Which line was selected?
				if (wmouse_trafo(sm_scroll->listWin, 
								 &event->y, &event->x, FALSE)) {
					
					// Set it as current
					setCDKScrollCurrent(sm_scroll, event->y);
					
					// Which one to hilight?
					if (!left)
						ungetch(KEY_LEFT);
					else
						ungetch(KEY_RIGHT);
					left ^= 1;
					
					// Send Focus
					my_setCDKFocusCurrent(cdkscreen, ObjPtr(sm_scroll));
					return 0;
					
				}
				
			}
			
			if (wenclose(connbutton->win, event->y, event->x)) {
				my_setCDKFocusCurrent(cdkscreen, ObjPtr(connbutton));
				ungetch(KEY_ENTER);
				return 0;
			}
			
			if (wenclose(addbutton->win, event->y, event->x)) {
				my_setCDKFocusCurrent(cdkscreen, ObjPtr(addbutton));
				ungetch(KEY_ENTER);
				return 0;
			}
			
			if (wenclose(editbutton->win, event->y, event->x)) {
				my_setCDKFocusCurrent(cdkscreen, ObjPtr(editbutton));
				ungetch(KEY_ENTER);
				return 0;
			}
			
			if (wenclose(delbutton->win, event->y, event->x)) {
				my_setCDKFocusCurrent(cdkscreen, ObjPtr(delbutton));
				ungetch(KEY_ENTER);
				return 0;
			}

		} // Left mouse
		
	} // SITEMGR
	
	// If editsite is up, check clicks.
	if (visible & VISIBLE_EDITSITE) {

		if(event->bstate & BUTTON1_CLICKED) {
			
			if (wenclose(edit_matrix->win, event->y, event->x)) {
				
				my_setCDKFocusCurrent(cdkscreen, ObjPtr(edit_matrix));
				
#if 0
				// This code is broken
				// We need to find which cell to activate now.
				for (r = 1; r <= edit_matrix->vrows; r++) {
					for (c = 1; c <= edit_matrix->vcols; c++) {
						if (wenclose(MATRIX_CELL(edit_matrix,r,c), 
									 event->y, event->x)) {
							fprintf(d, "clicked at %dx%d\n", r, c);
							jumpToCell(edit_matrix, 
									   r + edit_matrix->trow - 1, 
									   c);
						}
					} // for c
				} // for r
#endif
				
			} // in matrix
			
			
			if (wenclose(savebutton->win, event->y, event->x)) {
				my_setCDKFocusCurrent(cdkscreen, ObjPtr(savebutton));
				ungetch(KEY_ENTER);
				return 0;
			}
			
			if (wenclose(cancelbutton->win, event->y, event->x)) {
				my_setCDKFocusCurrent(cdkscreen, ObjPtr(cancelbutton));
				ungetch(KEY_ENTER);
				return 0;
			}
			
			if (wenclose(addkpbutton->win, event->y, event->x)) {
				my_setCDKFocusCurrent(cdkscreen, ObjPtr(addkpbutton));
				ungetch(KEY_ENTER);
				return 0;
			}
			
			if (wenclose(delkpbutton->win, event->y, event->x)) {
				my_setCDKFocusCurrent(cdkscreen, ObjPtr(delkpbutton));
				ungetch(KEY_ENTER);
				return 0;
			}
			
		} // Left mouse

	}


	return 1;
}



site_t *sitemgr_find(char *name)
{
	site_t *node;

	for (node = sites; node; node = node->next) {

		if (!mystrccmp(name, node->name)) return node;

	}

	return NULL;
}




void sitemgr_poll(void)
{
	char *dir;
	char newname[100];
	int i;

	switch(connect_edit) {
	case 1: // display siteedit
		connect_edit = 0;

		visible &= ~VISIBLE_SITEMGR;
		visible |= VISIBLE_EDITSITE;
		visible_draw();
		break;

	case 2: // save button psuhed
		sitemgr_save();

		/* Fall-Through */

	case 3: // cancel button pushed
		connect_edit = 0;

		botbar_print("");

		visible &= ~VISIBLE_EDITSITE;
		visible |= VISIBLE_SITEMGR;
		visible_draw();
		break;

	case 4: // delete button
		connect_edit = 0;

		if (left_site && (left_site->siteid != -1))
			lion_printf(engine_handle, "SITEDEL|SITEID=%u\r\n",
						left_site->siteid);

		visible &= ~VISIBLE_SITEMGR;
		visible_draw();

		visible = VISIBLE_SITEMGR;
		visible_draw();
		break;


	case 5: // copy button pushed
		connect_edit = 0;

		// remove SITEID to change save into ADD
		// Change the name
		// Save sites
		// Reload sitemgr
		if (!left_site) break;

		left_site->siteid = -1;  // Make it save, over sitemod

		// Find a new valid name (but give up at 99)
		for (i = 1; i < 100; i++) {
			snprintf(newname, sizeof(newname),
					 "Copy %d of %s",
					 i, 
					 left_site->name);

			if (!sitemgr_find(newname)) break;
		}

		SAFE_COPY(left_site->name, newname);

		// Go into Edit mode
		visible &= ~VISIBLE_SITEMGR;
		visible |= VISIBLE_EDITSITE;
		visible_draw();

		// Since SAVE only sends what has changed after edit, we need to make
		// it appear to be all (but defaults). We create a new node for this.
		left_site = sitemgr_newnode();

		// Chain it in so it will be released.
		left_site->next = sites;
		sites = left_site;

		break;
	}



	if (connect_ok) {
		
		connect_ok = 0;

		botbar_print("Connecting...");

		// Connect, OR, trigger PWD (hence DIRLIST)
		if (left_site) {
			// Check if they have set STARTDIR, which has become the defacto
			// standard for clients.
			dir = parser_findkey(left_site->keys, left_site->values, 
								 left_site->items, "STARTDIR");
			engine_set_startdir(LEFT_SID, dir);
			engine_connect_left(left_site->siteid, left_site->name);
		} else
			engine_pwd(LEFT_SID);

		if (right_site) {
			// Check if they have set STARTDIR, which has become the defacto
			// standard for clients.
			dir = parser_findkey(right_site->keys, right_site->values, 
								 right_site->items, "STARTDIR");
			engine_set_startdir(RIGHT_SID, dir);
			engine_connect_right(right_site->siteid, right_site->name);
		} else
			engine_pwd(RIGHT_SID);

		visible = VISIBLE_DISPLAY;
		visible_draw();

		left_site = NULL;
		right_site= NULL;

		sitemgr_freeallnodes();

	}
	
}























/**
 **
 ** EDIT SITE
 **
 **/

static int input_yna(int r, int input)
{
	int v;

	switch(input) {
	case '0':
	case 'n':
	case 'N':
		v = 0; 
		break;
	case 'y':
	case 'Y':
	case '1':
		v = 1; 
		break;
	case 'a':
	case 'A':
	case '2':
		v = 2; 
		break;
	case ' ':
		v = str2yna(getCDKMatrixCell(edit_matrix, r, 2));
		if (++v > 2) v = 0;
		break;
	}

	setCDKMatrixCell(edit_matrix, r, 2, yna2str(v));
	drawCDKMatrix (edit_matrix, FALSE);


	return 0;
}



static int matrix_callback(EObjectType cdktype, void *object, 
						   void *clientData, chtype input)
{
	CDKMATRIX *matrix = (CDKMATRIX *) object;
	MEVENT event;
	int c,r;

	if (input == KEY_MOUSE) 
		if (getmouse(&event) == OK) {
			return check_all_mouse(&event);
		} // get event

#define isChar(c)            ((int)(c) >= 0 && (int)(c) < KEY_MIN)

	// Ok keys
	if (isChar(input)) {

		c = getCDKMatrixCol(matrix);
		r = getCDKMatrixRow(matrix);

		// If we are in the fields that you may not edit...
		if ((c == 1) &&
			(r <= 23))
			return 0;
		
		if (c == 2) {
			switch(r) { // Some items are YNA, and some ints.
			case 8:
			case 9:
			case 10:
			case 11:
			case 12:
			case 13:
			case 14:
			case 15:
			case 22:
			case 23:
				return input_yna(r, input);
			case 3:
			case 7:
				if (!isdigit(input))
					return 0;
			} // switch r, - yna and port types

		} // if we are in column 2

	}

	if (input == KEY_ENTER) {
	}

	return 1;
}





static int save_callback(EObjectType cdktype, void *object, 
						 void *clientData, chtype input)
{
	MEVENT event;
	
	if (input == KEY_MOUSE) 
		if (getmouse(&event) == OK) {
			return check_all_mouse(&event);
		} // get event
	
	if (input == KEY_ENTER) {
		connect_edit = 2;
	}

	return 1;
}

static int cancel_callback(EObjectType cdktype, void *object, 
						 void *clientData, chtype input)
{
	MEVENT event;
	
	if (input == KEY_MOUSE) 
		if (getmouse(&event) == OK) {
			return check_all_mouse(&event);
		} // get event
	
	if (input == KEY_ENTER) {
		connect_edit = 3;
	}

	return 1;
}



static int addkp_callback(EObjectType cdktype, void *object, 
						 void *clientData, chtype input)
{
	MEVENT event;
	char **ktmp;
	
	if (input == KEY_MOUSE) 
		if (getmouse(&event) == OK) {
			return check_all_mouse(&event);
		} // get event
	

	if (!left_site) return 1;

	if (input == KEY_ENTER) {
		// Increase the number of client key/pair lines we have by one
		// Allocate one more space in keys and values, increase item
		// count, then redraw the edit window.
		
		ktmp = realloc(left_site->keys,
					   sizeof(char *) * (left_site->items + 1));
		if (!ktmp) return 1;
		left_site->keys = ktmp;

		ktmp = realloc(left_site->values,
					   sizeof(char *) * (left_site->items + 1));
		if (!ktmp) return 1;
		left_site->values = ktmp;

		left_site->keys  [ left_site->items ] = strdup("");
		left_site->values[ left_site->items ] = strdup("");
		
		left_site->items++;
		matrix_numkp++;

		sitemgr_undraw_edit();
		sitemgr_draw_edit();

		return 1;

	}

	return 1;
}




static int delkp_callback(EObjectType cdktype, void *object, 
						  void *clientData, chtype input)
{
	MEVENT event;
	
	if (input == KEY_MOUSE) 
		if (getmouse(&event) == OK) {
			return check_all_mouse(&event);
		} // get event
	

	if (!left_site) return 1;

	if (input == KEY_ENTER) {
		// Decrease the number of items, and free old data.

		if (left_site->items) {

			//left_site->items--;
			if (matrix_numkp > 0) { 
				matrix_numkp--;
				setCDKMatrixCell(edit_matrix, 24+matrix_numkp, 2, "");
			}
			//SAFE_FREE(left_site->keys  [ left_site->items ]);
			//SAFE_FREE(left_site->values[ left_site->items ]);
			
			//if (!left_site->items) {
			//	SAFE_FREE(left_site->keys);
			//	SAFE_FREE(left_site->values);
			//}

			//sitemgr_undraw_edit();
			//sitemgr_draw_edit();

		}

		return 1;

	}

	return 1;
}












void sitemgr_draw_edit(void)
{
	// FXP.One 0.1 defines 23 fields in sites, PLUS client supplied pairs.
	// Allocate strings for it.
	char **strings;
	int items, i;
	char *col_msg[3] = { "Not used", "Keys", "Values" };
	int col_sizes[3] = { 0, 15, COLS-15-10-12 }; // -10 in lines and whitespace
	int col_types[3] = { vCHAR, vMIXED, vMIXED};
	char inte[1024];
	char *save_msg[1] = { " Save"};
	char *cancel_msg[1] = { " Cancel"};
	char *addkp_msg[1] = { " Add K/P"};
	char *delkp_msg[1] = { " Del K/P"};

	items = 23;
	if (left_site)
		items += left_site->items;

	strings = calloc(sizeof(char *) * (items + 1), 1); // matrix start at 1
	if (!strings) return;

	if (left_site && (left_site->siteid != -1))
		botbar_printf("Edit site '%s'...", left_site->name);
	else
		botbar_print("Add site... (Required fields are: Name, Host, User, Pass)");


	savebutton   = newCDKButtonbox(cdkscreen, RIGHT, 2, 1, 10, NULL, 1, 1,
								   save_msg, 1, A_BOLD, TRUE, FALSE);
	cancelbutton = newCDKButtonbox(cdkscreen, RIGHT, 5, 1, 10, NULL, 1, 1,
								   cancel_msg, 1, A_BOLD, TRUE, FALSE);
	addkpbutton  = newCDKButtonbox(cdkscreen, RIGHT, 8, 1, 10, NULL, 1, 1,
								   addkp_msg, 1, A_BOLD, TRUE, FALSE);
	delkpbutton  = newCDKButtonbox(cdkscreen, RIGHT, 11, 1, 10, NULL, 1, 1,
								   delkp_msg, 1, A_BOLD, TRUE, FALSE);
								 

	{
		char *msg[1] = { "Skiplist, passlist & movefirst use '/' as seperator. Eg: *ignore*/*.dat/bad*" };

		nfo_msg = newCDKLabel(cdkscreen,
							  LEFT, LINES-2,
							  msg,
							  1,
							  FALSE,
							  FALSE);

		if (!nfo_msg) goto fail;
	}







	// Here, if we are Adding a site, we assign a new (blank) node to
	// left.
	if (!left_site)
		left_site = sitemgr_newnode();



	// We dont use titles, set them to blank.
	for (i = 0; i <= items; i++)
		strings[i] = " ";

	edit_matrix = newCDKMatrix(cdkscreen,
							   0, 2,           // x y
							   items, 2,       // actual size
							   LINES/2-4, 2,  // screen size
							   NULL,
							   strings,
							   col_msg,
							   col_sizes,
							   col_types,
							   -1,-1,       // spacing
							   '.',
							   ROW,
							   TRUE,
							   FALSE,
							   FALSE);
	if (!edit_matrix) goto fail;

	SAFE_FREE(strings);

	setCDKMatrixCell(edit_matrix, 1,  1, "Name");
	setCDKMatrixCell(edit_matrix, 2,  1, "Host");
	setCDKMatrixCell(edit_matrix, 3,  1, "Port");
	setCDKMatrixCell(edit_matrix, 4,  1, "User");
	setCDKMatrixCell(edit_matrix, 5,  1, "Pass");
	setCDKMatrixCell(edit_matrix, 6,  1, "iface");
	setCDKMatrixCell(edit_matrix, 7,  1, "iport");
	setCDKMatrixCell(edit_matrix, 8,  1, "Passive");
	setCDKMatrixCell(edit_matrix, 9,  1, "FXP Passive");
	setCDKMatrixCell(edit_matrix, 10, 1, "Control TLS");
	setCDKMatrixCell(edit_matrix, 11, 1, "Data TLS");
	setCDKMatrixCell(edit_matrix, 12, 1, "Binary Mode");
	setCDKMatrixCell(edit_matrix, 13, 1, "Resume");
	setCDKMatrixCell(edit_matrix, 14, 1, "Resume Last");
	setCDKMatrixCell(edit_matrix, 15, 1, "PRET");
	setCDKMatrixCell(edit_matrix, 16, 1, "File Skiplist");
	setCDKMatrixCell(edit_matrix, 17, 1, "Dir Skiplist");
	setCDKMatrixCell(edit_matrix, 18, 1, "File Passlist");
	setCDKMatrixCell(edit_matrix, 19, 1, "Dir Passlist");
	setCDKMatrixCell(edit_matrix, 20, 1, "File Movefirst");
	setCDKMatrixCell(edit_matrix, 21, 1, "Dir Movefirst");
	setCDKMatrixCell(edit_matrix, 22, 1, "File Skipempty");
	setCDKMatrixCell(edit_matrix, 23, 1, "Dir Skipempty");

#define X(S) (S ? S : "")

	if (left_site) {

		setCDKMatrixCell(edit_matrix,  1,  2, X(left_site->name));
		setCDKMatrixCell(edit_matrix,  2,  2, X(left_site->host));
		snprintf(inte, sizeof(inte), "%u", left_site->port);
		setCDKMatrixCell(edit_matrix,  3,  2, inte);
		setCDKMatrixCell(edit_matrix,  4,  2, X(left_site->user));
		setCDKMatrixCell(edit_matrix,  5,  2, X(left_site->pass));
		setCDKMatrixCell(edit_matrix,  6,  2, X(left_site->iface));
		snprintf(inte, sizeof(inte), "%u", left_site->iport);
		setCDKMatrixCell(edit_matrix,  7,  2, inte);
		setCDKMatrixCell(edit_matrix,  8,  2, yna2str(left_site->passive));
		setCDKMatrixCell(edit_matrix,  9,  2, yna2str(left_site->fxp_passive));
		setCDKMatrixCell(edit_matrix, 10,  2, yna2str(left_site->control_TLS));
		setCDKMatrixCell(edit_matrix, 11,  2, yna2str(left_site->data_TLS));
		setCDKMatrixCell(edit_matrix, 12,  2, yna2str(left_site->desired_type));
		setCDKMatrixCell(edit_matrix, 13,  2, yna2str(left_site->resume));
		setCDKMatrixCell(edit_matrix, 14,  2, yna2str(left_site->resume_last));
		setCDKMatrixCell(edit_matrix, 15,  2, yna2str(left_site->pret));
		setCDKMatrixCell(edit_matrix, 16,  2, X(left_site->fskiplist));
		setCDKMatrixCell(edit_matrix, 17,  2, X(left_site->dskiplist));
		setCDKMatrixCell(edit_matrix, 18,  2, X(left_site->fpasslist));
		setCDKMatrixCell(edit_matrix, 19,  2, X(left_site->dpasslist));
		setCDKMatrixCell(edit_matrix, 20,  2, X(left_site->fmovefirst));
		setCDKMatrixCell(edit_matrix, 21,  2, X(left_site->dmovefirst));
		setCDKMatrixCell(edit_matrix, 22,  2, yna2str(left_site->fskipempty));
		setCDKMatrixCell(edit_matrix, 23,  2, yna2str(left_site->dskipempty));

#undef X

		matrix_numkp = left_site->items;

		for (i = 0; i < left_site->items; i++) {
			setCDKMatrixCell(edit_matrix, 24+i, 1, 
							 left_site->keys[i]);
			setCDKMatrixCell(edit_matrix, 24+i, 2, 
							 left_site->values[i]);
		}

	}








	setCDKMatrixPreProcess(edit_matrix, matrix_callback, NULL);
	setCDKButtonboxPreProcess(savebutton, save_callback, NULL);
	setCDKButtonboxPreProcess(cancelbutton, cancel_callback, NULL);

	setCDKButtonboxPreProcess(addkpbutton, addkp_callback, NULL);
	setCDKButtonboxPreProcess(delkpbutton, delkp_callback, NULL);

	my_setCDKFocusCurrent(cdkscreen, ObjPtr(edit_matrix));
	
	refreshCDKScreen(cdkscreen);


	return;

 fail:
	sitemgr_undraw_edit();
	return;
							   
}




void sitemgr_undraw_edit(void)
{

	if (edit_matrix) {
		destroyCDKMatrix(edit_matrix);
		edit_matrix = NULL;
	}

	if (savebutton) {
		destroyCDKButtonbox(savebutton);
		savebutton = NULL;
	}

	if (cancelbutton) {
		destroyCDKButtonbox(cancelbutton);
		cancelbutton = NULL;
	}

	if (addkpbutton) {
		destroyCDKButtonbox(addkpbutton);
		addkpbutton = NULL;
	}

	if (delkpbutton) {
		destroyCDKButtonbox(delkpbutton);
		delkpbutton = NULL;
	}

	if (nfo_msg) {
		destroyCDKLabel(nfo_msg);
		nfo_msg = NULL;
	}

}




//
// Save the edited/new site. Work out what fields have changed from
// old/default site and send command.
//
void sitemgr_save(void)
{
	char *str;
	int i;
	char *key, *val;

	if (!left_site) return;

	// Check all fields, and save any thats new/changed.
	if (left_site->siteid == -1)
		lion_printf(engine_handle, "SITEADD");
	else
		lion_printf(engine_handle, "SITEMOD|SITEID=%d", left_site->siteid);

#define CHK(N,S,A) \
    str = getCDKMatrixCell(edit_matrix, (N), 2); \
    if (!str) str = ""; \
	if ((str && (S) && strcmp( (S) , str)) || \
		(!(S) && str && *str)) \
			lion_printf(engine_handle, (A), str);
#define CHK2(N,S,A) \
    str = getCDKMatrixCell(edit_matrix, (N), 2); \
    if (!str) { if ((N)==3) { str="21"; } else { str="0"; } } \
	if ((S) != atoi(str)) \
			lion_printf(engine_handle, (A), str);
#define CHK3(N,S,A) \
    str = getCDKMatrixCell(edit_matrix, (N), 2); \
    if (!str) str = "AUTO"; \
	if (((S) != str2yna(str))) \
			lion_printf(engine_handle, (A), str);

	CHK(  1,left_site->name,"|NAME=%s");
    CHK(  2,left_site->host,"|HOST=%s");
	CHK2( 3,left_site->port,"|PORT=%s");
	CHK(  4,left_site->user,"|USER=%s");
	CHK(  5,left_site->pass,"|PASS=%s");
	CHK(  6,left_site->iface,"|IFACE=%s");
	CHK2( 7,left_site->iport,"|IPORT=%s");
	CHK3( 8,left_site->passive,"|PASSIVE=%s");
	CHK3( 9,left_site->fxp_passive,"|FXP_PASSIVE=%s");
	CHK3(10,left_site->control_TLS,"|CONTROL_TLS=%s");
	CHK3(11,left_site->data_TLS,"|DATA_TLS=%s");
	CHK3(12,left_site->desired_type,"|DESIRED_TYPE=%s");
	CHK3(13,left_site->resume,"|RESUME=%s");
	CHK3(14,left_site->resume_last,"|RESUME_LAST=%s");
	CHK3(15,left_site->pret,"|PRET=%s");
	CHK(16,left_site->fskiplist,"|FSKIPLIST=%s");
	CHK(17,left_site->dskiplist,"|DSKIPLIST=%s");
	CHK(18,left_site->fpasslist,"|FPASSLIST=%s");
	CHK(19,left_site->dpasslist,"|DPASSLIST=%s");
	CHK(20,left_site->fmovefirst,"|FMOVEFIRST=%s");
	CHK(21,left_site->dmovefirst,"|DMOVEFIRST=%s");
    CHK3(22,left_site->fskipempty,"|FSKIPEMPTY=%s");
    CHK3(23,left_site->dskipempty,"|DSKIPEMPTY=%s");

#undef CHK
#undef CHK2

	// Also loop through the extras.
	for (i = 0; i < matrix_numkp; i++) {

		// If these are MORE than we had before, its plain add.
		if ((i >= left_site->items) &&
			(key = getCDKMatrixCell(edit_matrix, 24+i, 1)) &&
			(val = getCDKMatrixCell(edit_matrix, 24+i, 2))) {
			lion_printf(engine_handle, "|%s=%s", key, val);
			continue;
		}

		// Check if it has change. If "key" is different, we need
		// to wipe old key (no value), then send new key.
		key = getCDKMatrixCell(edit_matrix, 24+i, 1);
		val = getCDKMatrixCell(edit_matrix, 24+i, 2);
		if (!key) key = "";
		if (!val) val = "";

		if (left_site->keys[i][0] && mystrccmp(left_site->keys[i], key)) {
			lion_printf(engine_handle, "|%s=", left_site->keys[i]);
			lion_printf(engine_handle, "|%s=%s", key, val);
			continue;
		}

		// If just value is different, send it.
		if (mystrccmp(left_site->values[i], val)) {
			lion_printf(engine_handle, "|%s=%s", key, val);
			continue;
		}

	}

	// Now check if site->items is BIGGER than matrix, if so, delete the
	// extra items.
	// Can never happen.
	for( ; i < left_site->items; i++)
		lion_printf(engine_handle, "|%s=", left_site->keys[i]);


    lion_printf(engine_handle, "\r\n");

}
