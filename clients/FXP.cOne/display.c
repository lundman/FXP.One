
#include <stdio.h>
#include <cdk/cdk.h>

#include "lion.h"
#include "misc.h"

#include "traverse.h"
#include "main.h"
#include "engine.h"
#include "display.h"
#include "file.h"

/**
-.-Left------------------------------Menu-----------------------------Right-.-
 Path: /                                Path: /tv/
a-------------------------------------++-------------------------------------+
|Some.Release.Name-DVDR/              ||Other.files                          |
|NetBSD-2.0.2/                        ||                                     |
|                                     ||                                     |
|                                     ||                                     |
|                                     ||                                     |
|                                     ||                                     |
|                                     ||                                     |
|                                     ||                                     |
|                                     ||                                     |
|                                     ||                                     |
|                                     ||                                     |
|                                     ||                                     |
|                                     ||                                     |
|                                     ||                                     |
|                                     ||                                     |
|                                     ||                                     |
|                                     ||                                     |
|                                     ||                                     |
|                                     ||                                     |
+-------------------------------------++-------------------------------------+
drwx-r-xr-x user group   4123422 Jun 17 17:22 Some.Release.Name-DVDR/
                                                                              
-rwx-r-xr-x user group       512 Jun 11 11:12 Other.files
                         
-.--------------------------------------------------------------------------.-
**/


static CDKSELECTION  *l_files        = NULL;
static CDKSELECTION  *r_files        = NULL;
static CDKENTRY      *l_path         = NULL;
static CDKENTRY      *r_path         = NULL;
static CDKLABEL      *l_details      = NULL;
static CDKLABEL      *r_details      = NULL;
static CDKITEMLIST   *l_sortby       = NULL;
static CDKITEMLIST   *r_sortby       = NULL;
static CDKLABEL      *left_sitename  = NULL;
static CDKLABEL      *right_sitename = NULL;


static int begx, begy, height, width;

static file_t     **l_the_files      = NULL;
static unsigned int l_num_files      = 0;
static unsigned int l_num_allocated  = 0;

static file_t     **r_the_files      = NULL;
static unsigned int r_num_files      = 0;
static unsigned int r_num_allocated  = 0;

static sortby_t l_sortby_setting     = SORT_DATE;
static sortby_t r_sortby_setting     = SORT_DATE;

static unsigned int l_clear          = 0;
static unsigned int r_clear          = 0;

static int focus_disabled = 0;

// 1 Leave and call GO
static int display_done = 0;



static int l_path_callback(EObjectType cdktype, void *object, 
						   void *clientData, chtype input)
{
	CDKENTRY *path = (CDKENTRY *)object;
	MEVENT event;
	char *pwd;

	if (input == KEY_MOUSE) 
		if (getmouse(&event) == OK) 
				return check_all_mouse(&event);

	// Attempt CWD
	switch(input) {

	case KEY_ENTER:
		pwd = getCDKEntryValue(path);
		if (pwd) {
			engine_cwd(LEFT_SID, pwd);
			l_clear = 1;
		}
		break;
	case KEY_F(1):
	case KEY_DOWN:
		if (l_files) my_setCDKFocusCurrent(cdkscreen, ObjPtr(l_files));
		return 0;
		
	case KEY_F(2):
		if (r_files) my_setCDKFocusCurrent(cdkscreen, ObjPtr(r_files));
		return 0;

	case KEY_F(5):
		display_refresh(LEFT_SID);
		break;
	}
	

	return 1;
}


static int r_path_callback(EObjectType cdktype, void *object, 
						   void *clientData, chtype input)
{
	CDKENTRY *path = (CDKENTRY *)object;
	MEVENT event;
	char *pwd;

	if (input == KEY_MOUSE) 
		if (getmouse(&event) == OK) 
				return check_all_mouse(&event);

	// Attempt CWD
	switch( input ) {
	case KEY_ENTER:
		pwd = getCDKEntryValue(path);
		if (pwd) {
			engine_cwd(RIGHT_SID, pwd);
			r_clear = 1;
		}
		break;
	case KEY_F(1):
		if (l_files) my_setCDKFocusCurrent(cdkscreen, ObjPtr(l_files));
		return 0;
	case KEY_F(2):
	case KEY_DOWN:
		if (r_files) my_setCDKFocusCurrent(cdkscreen, ObjPtr(r_files));
		return 0;
	case KEY_F(5):
		display_refresh(RIGHT_SID);
		break;

	}
	

	return 1;
}



static int l_presortby_callback(EObjectType cdktype, void *object, 
								void *clientData, chtype input)
{
	//CDKITEMLIST *path = (CDKITEMLIST *)object;
	MEVENT event;

	if (input == KEY_MOUSE) 
		if (getmouse(&event) == OK) 
			return check_all_mouse(&event);

	switch(input) {
	case KEY_F(1):
		if (l_files) my_setCDKFocusCurrent(cdkscreen, ObjPtr(l_files));
		return 0;
	case KEY_F(2):
		if (r_files) my_setCDKFocusCurrent(cdkscreen, ObjPtr(r_files));
		return 0;
	case KEY_F(5):
		display_refresh(LEFT_SID);
		break;
	}
	
	return 1;
}



static int r_presortby_callback(EObjectType cdktype, void *object, 
								void *clientData, chtype input)
{
	//CDKITEMLIST *path = (CDKITEMLIST *)object;
	MEVENT event;

	if (input == KEY_MOUSE) 
		if (getmouse(&event) == OK) 
			return check_all_mouse(&event);
	
	switch(input) {
	case KEY_F(1):
		if (l_files) my_setCDKFocusCurrent(cdkscreen, ObjPtr(l_files));
		return 0;
	case KEY_F(2):
		if (r_files) my_setCDKFocusCurrent(cdkscreen, ObjPtr(r_files));
		return 0;
	case KEY_F(5):
		display_refresh(RIGHT_SID);
		break;
	}

	return 1;
}




static int l_sortby_callback(EObjectType cdktype, void *object, 
							 void *clientData, chtype input)
{
	CDKITEMLIST *path = (CDKITEMLIST *)object;
	sortby_t current;

	current = getCDKItemlistCurrentItem(path);
	if (!l_the_files) return 1;

	if (current != l_sortby_setting) {
		l_sortby_setting = current;
		display_sort_files(LEFT_SID, 1);
	}


	return 1;
}


static int r_sortby_callback(EObjectType cdktype, void *object, 
							 void *clientData, chtype input)
{
	CDKITEMLIST *path = (CDKITEMLIST *)object;
	sortby_t current;

	current = getCDKItemlistCurrentItem(path);
	if (!r_the_files) return 1;

	if (current != r_sortby_setting) {
		r_sortby_setting = current;
		display_sort_files(RIGHT_SID, 1);
	}


	return 1;
}











static int l_files_callback(EObjectType cdktype, void *object, 
						  void *clientData, chtype input)
{
	CDKSELECTION *path = (CDKSELECTION *)object;
	MEVENT event;
	unsigned int i;


	if (input == KEY_MOUSE) 
		if (getmouse(&event) == OK) 
			return check_all_mouse(&event);


	i = path->currentItem;

	switch(input) {

	case KEY_ENTER:
		// Is it a dir
		if (i >= l_num_files) return 1;

		if (l_the_files[i]->directory == YNA_YES) {
			engine_cwd(LEFT_SID, l_the_files[i]->name);
			// We can't free this object here, as we are in the
			// event handler of this object.
			l_clear = 1;
		}
		break;

	case ' ':
		if (i >= l_num_files) return 1;

		if (!i) {
			ungetch('c');
			return 0; // The ".."
		}

		if (l_the_files[i]->queued == YNA_YES) {
			if (getCDKSelectionChoice(path, i) == 3)
				setCDKSelectionChoice(path, i, 1);
			return 1;
		}

		if (getCDKSelectionChoice(path, i) == 1)
			setCDKSelectionChoice(path, i, 3);
		break;

	case 'c':
	case 'C':
		display_clear_selected(LEFT_SID);
		break;

	case CTRL('G'):
		display_done = 1;
		break;

	case 'q':
	case 'Q':
		display_queue_selected(LEFT_SID);
		break;

	case KEY_F(2):
		if (r_files) my_setCDKFocusCurrent(cdkscreen, ObjPtr(r_files));
		return 0;

	case KEY_F(5):
		display_refresh(LEFT_SID);
		break;
	
	}
	

	return 1;
}





static int r_files_callback(EObjectType cdktype, void *object, 
						  void *clientData, chtype input)
{
	CDKSELECTION *path = (CDKSELECTION *)object;
	MEVENT event;
	int i;

	if (input == KEY_MOUSE) 
		if (getmouse(&event) == OK) 
			return check_all_mouse(&event);

	i = path->currentItem;

	switch(input) {

	case KEY_ENTER:
		// Is it a dir
		if (i >= r_num_files) return 1;

		if (r_the_files[i]->directory == YNA_YES) {
			engine_cwd(RIGHT_SID, r_the_files[i]->name);
			r_clear = 1;
		}
		break;

	case ' ':
		if (i >= r_num_files) return 1;

		if (!i) {
			ungetch('c');
			return 0; // The ".."
		}

		if (r_the_files[i]->queued == YNA_YES) {
			if (getCDKSelectionChoice(path, i) == 3)
				setCDKSelectionChoice(path, i, 1);
			return 1;
		}

		if (getCDKSelectionChoice(path, i) == 1)
			setCDKSelectionChoice(path, i, 3);
		break;

	case 'c':
	case 'C':
		display_clear_selected(RIGHT_SID);
		break;

	case CTRL('G'):
		display_done = 1;
		break;

	case 'q':
	case 'Q':
		display_queue_selected(RIGHT_SID);
		break;

	case KEY_F(1):
		if (l_files) my_setCDKFocusCurrent(cdkscreen, ObjPtr(l_files));
		return 0;

	case KEY_F(5):
		display_refresh(RIGHT_SID);
		break;

	}


	return 1;
}



char *misc_file_date(time_t ftime)
{
  static char date[13];
  int i, j;
  char *longstring;
  
  j = 0;

  longstring = ctime(&ftime);

  for (i = 4; i < 11; ++i)
    date[j++] = longstring[i];
  
#define SIXMONTHS       ((365 / 2) * 60 * 60 * 24)

  if (ftime + SIXMONTHS > time(NULL))
    for (i = 11; i < 16; ++i)
      date[j++] = longstring[i];
  else {
    date[j++] = ' ';
    for (i = 20; i < 24; ++i)
      date[j++] = longstring[i];
  }
  date[j++] = 0;

  return date;
}






//
// Draw file details, if given.
//
static void display_details(CDKLABEL **label, file_t *file)
{
	char line1[512];
	char line2[512];
	char *msg[2];
	int width, len;

	width = COLS;

	msg[0] = line1;
	msg[1] = line2;

	if (!file) {
		snprintf(line1, sizeof(line1), 
				 "</57>%-*.*s<!57>",
				 width, width, " ");
		snprintf(line2, sizeof(line2), 
				 "</57>%-*.*s<!57>",
				 width, width, " ");
	} else {

		len = strlen(file->name) + 1; // for the " " or "/"
		
		snprintf(line1, sizeof(line1), 
				 "</57>%s%c%*.*s<!57>",
				 file->name,
				 file->directory == YNA_YES ? '/' : ' ',
				 width-len, width-len, " ");

		width-=(10+1+12+1+12+12+1+15+1);
		snprintf(line2, sizeof(line2), 
				 "</57>%-10.10s %-12.12s %-12.12s %12"PRIu64" %15.15s %*.*s<!57>",
				 file->perm, file->user, file->group, file->size,
				 misc_file_date(file->date), width, width, " ");
	}

	if (!*label)
		*label = newCDKLabel(cdkscreen, LEFT, 
							*label == l_details ? height-3 : height-1, // Ugh!
							msg, 2, FALSE, FALSE);
	else
		setCDKLabelMessage(*label, msg, 2);

}





static int l_details_callback(EObjectType cdktype, void *object, 
							  void *clientData, chtype input)
{
	CDKSELECTION *path = (CDKSELECTION *)object;
	unsigned int i;

	i = path->currentItem;

	if (i >= l_num_files) return 1;

	switch(input) {

	case KEY_DOWN:
	case KEY_UP:
		display_details(&l_details, !i ? NULL : l_the_files[i]);
		break;
	case KEY_F(1):
		if (l_files) my_setCDKFocusCurrent(cdkscreen, ObjPtr(l_files));
		break;
	case KEY_F(2):
		if (r_files) my_setCDKFocusCurrent(cdkscreen, ObjPtr(r_files));
		break;
	}
	

	return 1;
}


static int r_details_callback(EObjectType cdktype, void *object, 
							  void *clientData, chtype input)
{
	CDKSELECTION *path = (CDKSELECTION *)object;
	unsigned int i;

	i = path->currentItem;

	if (i >= r_num_files) return 1;

	switch(input) {

	case KEY_DOWN:
	case KEY_UP:
		display_details(&r_details, !i ? NULL : r_the_files[i]);
		break;
	case KEY_F(1):
		if (l_files) my_setCDKFocusCurrent(cdkscreen, ObjPtr(l_files));
		break;
	case KEY_F(2):
		if (r_files) my_setCDKFocusCurrent(cdkscreen, ObjPtr(r_files));
		break;
	}
	

	return 1;
}








//
// We received QADD back from FXP.One, so we can now tag this item
// as queued.
//
void display_queued_item(int side, unsigned int fid)
{
	unsigned int num_files;
	file_t **files;
	CDKSELECTION *list;
	int i;

	if (side == LEFT_SID) {
		num_files = l_num_files;
		files = l_the_files;
		list = l_files;
	} else {
		num_files = r_num_files;
		files = r_the_files;
		list = r_files;
	}


	for (i = 1; i < num_files; i++) {
		if (files[i]->fid == fid) {
			setCDKSelectionChoice(list, i, 3);
			files[i]->queued = YNA_YES;
			drawCDKSelection(list, TRUE);
			return;
		}
	}
}


void display_queue_selected(int side)
{
	unsigned int num_files;
	file_t **files;
	CDKSELECTION *list;
	int i, items = 0;

	// Attempt to loop through all selected items and queue.
	// If engine_queue() returns failure, we stop and exit. It 
	// actually is creating a queue and will call us back here again
	// if the queue creation went ok. and we will do it again.
	if (side == LEFT_SID) {
		num_files = l_num_files;
		files = l_the_files;
		list = l_files;
	} else {
		num_files = r_num_files;
		files = r_the_files;
		list = r_files;
	}


	for (i = 1; i < num_files; i++) {
		if (getCDKSelectionChoice(list, i)&1) { // Selected
			if (!engine_queue(side, files[i]->fid)) break; // Stop!
			items++;
		}
	}

	if (items)
		botbar_printf("Queued %d item%s",
					  items,
					  items == 1 ? "" : "s");
}





void display_iterate_selected(int side, int (*callback)(char *), int type)
{
	unsigned int num_files;
	file_t **files;
	CDKSELECTION *list;
	int i;

	if (side == LEFT_SID) {
		num_files = l_num_files;
		files = l_the_files;
		list = l_files;
	} else {
		num_files = r_num_files;
		files = r_the_files;
		list = r_files;
	}


	switch(type) {
	case DISPLAY_ALL:
		for (i = 1; i < num_files; i++) {
			if (getCDKSelectionChoice(list, i)&1) { // Selected
				if (!callback(files[i]->name)) break; // Stop!
			}
		}
		break;

	case DISPLAY_ONLYFILES:
		for (i = 1; i < num_files; i++) {
			if ((files[i]->directory != YNA_YES) &&
				getCDKSelectionChoice(list, i)&1) { // Selected
				if (!callback(files[i]->name)) break; // Stop!
			}
		}
		break;

	case DISPLAY_ONLYDIRS:
		for (i = 1; i < num_files; i++) {
			if ((files[i]->directory == YNA_YES) &&
				getCDKSelectionChoice(list, i)&1) { // Selected
				if (!callback(files[i]->name)) break; // Stop!
			}
		}
		break;
	}
}







void display_select_fids(int side, char *line)
{
	static int left_count  = 0;
	static int right_count = 0;
	unsigned int num_files;
	file_t **files;
	CDKSELECTION *list;
	int *count, i;
	unsigned int fid;
	char *ar, *token;

	if (side == LEFT_SID) {
		num_files = l_num_files;
		files = l_the_files;
		list = l_files;
		count = &left_count;
	} else {
		num_files = r_num_files;
		files = r_the_files;
		list = r_files;
		count = &right_count;
	}

	if (!line) {
		botbar_printf("Left %u item%s selected.   Right %u item%s selected.",
					  left_count, left_count == 1 ? "" : "s",
					  right_count, right_count == 1 ? "" : "s");
		left_count = 0;
		right_count = 0;
		drawCDKSelection(l_files, TRUE);
		drawCDKSelection(r_files, TRUE);
		return;
	}



	// For all fids we can read, find it, select it.
	ar = line;

	while((token = misc_digtoken(&ar, ",\r\n"))) {

		fid = atoi(token);

		for (i = 1; i < num_files; i++) {
			if (files[i]->fid == fid) {
				
				if (files[i]->queued == YNA_YES) 
					setCDKSelectionChoice(list, i, 3);
				else
					setCDKSelectionChoice(list, i, 1);

				(*count)++;
				break;
			}
		}
	}
}






void display_clear_selected(int side)
{
	unsigned int num_files;
	file_t **files;
	CDKSELECTION *list;
	int i;

	if (side == LEFT_SID) {
		num_files = l_num_files;
		files = l_the_files;
		list = l_files;
	} else {
		num_files = r_num_files;
		files = r_the_files;
		list = r_files;
	}


	for (i = 1; i < num_files; i++) {
		if (files[i]->queued == YNA_YES)
			setCDKSelectionChoice(list, i, 2);			
		else
			setCDKSelectionChoice(list, i, 0);			
	}

}








int display_checkclick(MEVENT *event)
{

	if (focus_disabled) return 1;

	if(event->bstate & BUTTON1_CLICKED) {
		
		if (wenclose(l_sortby->win, event->y, event->x)) {
			my_setCDKFocusCurrent(cdkscreen, ObjPtr(l_sortby));
			ungetch(' ');
			return 0;
		}
		
		if (wenclose(r_sortby->win, event->y, event->x)) {
			my_setCDKFocusCurrent(cdkscreen, ObjPtr(r_sortby));
			ungetch(' ');
			return 0;
		}
	}

	if((event->bstate & BUTTON1_CLICKED) ||  
	   (event->bstate & BUTTON1_DOUBLE_CLICKED)) {
		if (wenclose(l_files->win, event->y, event->x)) {
			
			// Clicked in the itemlist, set focus and currentItem
			// then send space.
			my_setCDKFocusCurrent(cdkscreen, ObjPtr(l_files));

			if (wmouse_trafo(l_files->win, 
							 &event->y, &event->x, FALSE) &&
				(event->y > 0) &&
				(event->y < (height-7))&&
				(event->y <= l_num_files)) {

				setCDKSelectionCurrent(l_files, 
									   event->y-1 + l_files->currentTop);

				if(event->bstate & BUTTON1_CLICKED)
					ungetch(' ');
				else
					ungetch(KEY_ENTER);

			}

			return 0;
		}

	} // left button



	if((event->bstate & BUTTON1_CLICKED) ||  
	   (event->bstate & BUTTON1_DOUBLE_CLICKED)) {
		if (wenclose(r_files->win, event->y, event->x)) {
			
			// Clicked in the itemlist, set focus and currentItem
			// then send space.
			my_setCDKFocusCurrent(cdkscreen, ObjPtr(r_files));

			if (wmouse_trafo(r_files->win, 
							 &event->y, &event->x, FALSE) &&
				(event->y > 0) &&
				(event->y < (height-7))&&
				(event->y <= r_num_files)) {

				setCDKSelectionCurrent(r_files, 
									   event->y-1 + r_files->currentTop);

				if(event->bstate & BUTTON1_CLICKED)
					ungetch(' ');
				else
					ungetch(KEY_ENTER);

			}

			return 0;
		}

	} // left button




	return 1;
}



//
// We don't actually release the files ptr, and allocated, since
// we can use it again. When we remove the window we do.
//
void display_clear_lfiles(void)
{
	int i;

	for (i = 0; i < l_num_files; i++) {
		file_free(l_the_files[i]);
		l_the_files[i] = NULL;
	}
	l_num_files = 0;

}



void display_clear_rfiles(void)
{
	int i;

	for (i = 0; i < r_num_files; i++) {
		file_free(r_the_files[i]);
		r_the_files[i] = NULL;
	}
	r_num_files = 0;

}



void display_refresh(int side)
{

	if (side == LEFT_SID) {
		display_clear_lfiles();
		engine_pwd(LEFT_SID);
	}

	if (side == RIGHT_SID) {
		display_clear_rfiles();
		engine_pwd(RIGHT_SID);
	}

}




void display_set_lpath(char *path)
{

	if (!l_path) return;
	setCDKEntryValue(l_path, path);
	drawCDKEntry(l_path, FALSE);

}



void display_set_rpath(char *path)
{

	if (!r_path) return;
	setCDKEntryValue(r_path, path);
	drawCDKEntry(r_path, FALSE);

}






//
// Receive a file from the network, and inject it into our own
// lists.
//
void display_inject_file(int side, file_t *file)
{
	file_t ***files, **tmp;
	unsigned int *num_files;
	unsigned int *num_allocated;
	static file_t *parent = NULL; // static for recursion.



	if (side == LEFT_SID) {
		files         = &l_the_files;
		num_files     = &l_num_files;
		num_allocated = &l_num_allocated;
	} else if (side == RIGHT_SID) {
		files         = &r_the_files;
		num_files     = &r_num_files;
		num_allocated = &r_num_allocated;
	} else return;


	// If it is the very first inject, put in ".." first.
	if (!*num_files && !parent) {
		parent = file_new();
		file_strdup_name(parent, "..", "DIRECTORY");
		display_inject_file(side, parent);
		parent = NULL;
	}


	if (!file) return;

	
#define ALLOC_STEP_SIZE 20

	// Do we need to allocate more space?
	if (!*files || (*num_files >= *num_allocated)) {
		
		tmp = realloc(*files, sizeof(file_t *) * ((*num_allocated) + 
												  ALLOC_STEP_SIZE));
		if (!tmp) return;
		*files = tmp;
		(*num_allocated) += ALLOC_STEP_SIZE;

	}

	(*files)[ (*num_files) ] = file;
	(*num_files)++;

}














void display_set_lfiles(char *reason)
{
	unsigned int num_files;
	char *choice_msg[4] = { "</0> ", "</16></B>* ", " </5></B>>", "</16></B>*<!16></5>>"};
	char **tmp_files;
	char plswait[] = " <please wait...> ";
	char notconn[] = " <not connected> ";
	int i;
	int focus = 0;

	if (l_files) {

		if (getCDKFocusCurrent(cdkscreen) == ObjPtr(l_files))
			focus = 1;

		destroyCDKSelection(l_files);
		l_files = NULL;
	}

	if (!l_num_files) {

		num_files = 1;

	} else {

		num_files = l_num_files;

	}

	
	// Create the msg string.
	tmp_files = calloc(sizeof(char *) * num_files, 1);
	if (!tmp_files) 
		return;

	// Can't be empty in CDK.
	tmp_files[0] = notconn;
	
	if (engine_isconnected(LEFT_SID))
		tmp_files[0] = plswait;

	if (reason)
		tmp_files[0] = reason;

	
	for (i = 0; i < l_num_files; i++)
		tmp_files[i] = l_the_files[i]->display_name;
		

	l_files = newCDKSelection(cdkscreen, LEFT, begy+1,
							  RIGHT,
							  height - 6,
							  width/2,
							  NULL,
							  tmp_files, // sel
							  num_files,
							  choice_msg,
							  4,
							  A_BOLD|A_REVERSE,
							  TRUE,
							  FALSE);

	SAFE_FREE(tmp_files);

	if (l_files) {

		if (focus)
			my_setCDKFocusCurrent(cdkscreen, ObjPtr(l_files));

		setCDKSelectionPreProcess(l_files, l_files_callback, NULL);
		setCDKSelectionPostProcess(l_files, l_details_callback, NULL);
		drawCDKSelection(l_files, FALSE);

	}
}




void display_set_rfiles(char *reason)
{
	unsigned int num_files;
	char *choice_msg[4] = { "</0>  ", "</16></B> *", "</5></B><<!B><!5> ", "</5></B><<!5></16>*"};
	char **tmp_files;
	char plswait[] = " <please wait...> ";
	char notconn[] = " <not connected> ";
	int i;
	int focus = 0;

	if (r_files) {

		if (getCDKFocusCurrent(cdkscreen) == ObjPtr(r_files))
			focus = 1;
		
		destroyCDKSelection(r_files);
		r_files = NULL;
	}

	if (!r_num_files) {

		num_files = 1;

	} else {

		num_files = r_num_files;

	}

	
	// Create the msg string.
	tmp_files = calloc(sizeof(char *) * num_files, 1);
	if (!tmp_files) 
		return;

	// Can't be empty in CDK.
	tmp_files[0] = notconn;

	if (engine_isconnected(RIGHT_SID))
		tmp_files[0] = plswait;

	if (reason)
		tmp_files[0] = reason;

	
	for (i = 0; i < r_num_files; i++)
		tmp_files[i] = r_the_files[i]->display_name;
		

	r_files = newCDKSelection(cdkscreen, RIGHT, begy+1,
							  RIGHT,
							  height - 6,
							  width/2,
							  NULL,
							  tmp_files, // sel
							  num_files,
							  choice_msg,
							  4,
							  A_BOLD|A_REVERSE,
							  TRUE,
							  FALSE);

	SAFE_FREE(tmp_files);

	if (r_files) {

		if (focus)
			my_setCDKFocusCurrent(cdkscreen, ObjPtr(r_files));

		setCDKSelectionPreProcess(r_files, r_files_callback, NULL);
		setCDKSelectionPostProcess(r_files, r_details_callback, NULL);
		drawCDKSelection(r_files, FALSE);

	}
}

 



//
// Dirlisting complete, sort entries and display.
//
void display_set_files(int side, char *reason)
{

	if (!(visible & VISIBLE_DISPLAY)) return;

	if (side == LEFT_SID)
		display_set_lfiles(reason);
	else
		display_set_rfiles(reason);

}











int sortby_name(const void *data1, const void *data2)
{
  return (mystrccmp(
                 (*(file_t **)data1)->name,
                 (*(file_t **)data2)->name)
          );
}

int sortby_name_reverse(const void *data1, const void *data2)
{
  return -1*(mystrccmp(
                 (*(file_t **)data1)->name,
                 (*(file_t **)data2)->name)
          );
}

int sortby_size(const void *data1, const void *data2)
{
  if ( (*(file_t **)data1)->size >= 
       (*(file_t **)data2)->size)
    return -1;
  else 
    return 1;
}

int sortby_size_reverse(const void *data1, const void *data2)
{
  if ( (*(file_t **)data1)->size >= 
       (*(file_t **)data2)->size)
    return 1;
  else 
    return -1;
}


int sortby_date(const void *data1, const void *data2)
{
  if ( (*(file_t **)data1)->date >= 
       (*(file_t **)data2)->date)
    return -1;
  else 
    return 1;
}

int sortby_date_reverse(const void *data1, const void *data2)
{
  if ( (*(file_t **)data1)->date >= 
       (*(file_t **)data2)->date)
    return 1;
  else 
    return -1;
}

int sortby_user(const void *data1, const void *data2)
{
  return (mystrccmp(
                 (*(file_t **)data1)->user,
                 (*(file_t **)data2)->user)
          );
}

int sortby_user_reverse(const void *data1, const void *data2)
{
  return -1*(mystrccmp(
                 (*(file_t **)data1)->user,
                 (*(file_t **)data2)->user)
          );
}

int sortby_group(const void *data1, const void *data2)
{
  return (mystrccmp(
                 (*(file_t **)data1)->group,
                 (*(file_t **)data2)->group)
          );
}

int sortby_group_reverse(const void *data1, const void *data2)
{
  return -1*(mystrccmp(
                 (*(file_t **)data1)->group,
                 (*(file_t **)data2)->group)
          );
}






void display_sort_files(int side, int focus)
{
	sortby_t sortby_setting;
	void *files;
	unsigned int num_files;

	if (side == LEFT_SID) {
		if (!l_num_files) return;

		l_sortby_setting = getCDKItemlistCurrentItem(l_sortby);
		sortby_setting = l_sortby_setting;
		files = (void *)&l_the_files[1]; // Skip ".."
		num_files = l_num_files-1;

	} else {
		if (!r_num_files) return;

		r_sortby_setting = getCDKItemlistCurrentItem(r_sortby);
		sortby_setting = r_sortby_setting;
		files = (void *)&r_the_files[1];
		num_files = r_num_files-1;
	}


	switch(sortby_setting) {
	case SORT_NAME:
		qsort(files, num_files, sizeof(file_t *), 
			  sortby_name);
		break;
	case SORT_SIZE:
		qsort(files, num_files, sizeof(file_t *), 
			  sortby_size);
		break;
	case SORT_DATE:
		qsort(files, num_files, sizeof(file_t *), 
			  sortby_date);
		break;
	case SORT_USER:
		qsort(files, num_files, sizeof(file_t *), 
			  sortby_user);
		break;
	case SORT_GROUP:
		qsort(files, num_files, sizeof(file_t *), 
			  sortby_group);
		break;
	case SORT_RNAME:
		qsort(files, num_files, sizeof(file_t *), 
			  sortby_name_reverse);
		break;
	case SORT_RSIZE:
		qsort(files, num_files, sizeof(file_t *), 
			  sortby_size_reverse);
		break;
	case SORT_RDATE:
		qsort(files, num_files, sizeof(file_t *), 
			  sortby_date_reverse);
		break;
	case SORT_RUSER:
		qsort(files, num_files, sizeof(file_t *), 
			  sortby_user_reverse);
		break;
	case SORT_RGROUP:
		qsort(files, num_files, sizeof(file_t *), 
			  sortby_group_reverse);
		break;

	}

	display_set_files(side, NULL);


}





















void display_draw()
{

	botbar_print("");

	begx = 0;
	begy = 1;
	height = LINES-begy-1;
	width = COLS - 2;


	{
		char *sortby_msg[10] = {
			"</63>+Name", "</63>-Name", "</63>+Size", "</63>-Size",
			"</63>+Date", "</63>-Date", "</63>+User", "</63>-User", 
			"</63>+Group", "</63>-Group"};

		l_sortby = newCDKItemlist(cdkscreen, LEFT, height - 4, 
								  NULL, "</B>S<!B>ort by:",
								  sortby_msg, 10, 
								  l_sortby_setting, 
								  FALSE, FALSE);
		r_sortby = newCDKItemlist(cdkscreen, RIGHT, height - 4, 
								  NULL, "</B>S<!B>ort by:",
								  sortby_msg, 10, 
								  r_sortby_setting,
								  FALSE, FALSE);

		setCDKItemlistPreProcess(l_sortby, l_presortby_callback, NULL);
		setCDKItemlistPreProcess(r_sortby, r_presortby_callback, NULL);
		setCDKItemlistPostProcess(l_sortby, l_sortby_callback, NULL);
		setCDKItemlistPostProcess(r_sortby, r_sortby_callback, NULL);

	}


	display_set_lfiles(NULL);
	
	if (!l_files) goto fail;

	display_set_rfiles(NULL);

	if (!r_files) goto fail;


	// Top paths
	l_path = newCDKEntry(cdkscreen, LEFT, begy,
						 NULL,
						 "Path: ",
						 0,
						 ' ',
						 vMIXED,
						 COLS/2 - 6, // "Path: "
						 1,
						 256,
						 FALSE,
						 FALSE);
	if (!l_path) goto fail;

	r_path = newCDKEntry(cdkscreen, RIGHT, begy,
						 NULL,
						 "Path: ",
						 0,
						 ' ',
						 vMIXED,
						 COLS/2 - 6, // "Path: "
						 1,
						 256,
						 FALSE,
						 FALSE);
	if (!r_path) goto fail;


	setCDKEntryPreProcess(l_path, l_path_callback, NULL);
	setCDKEntryPreProcess(r_path, r_path_callback, NULL);


	{
		char *msg[1];

		msg[0] = engine_get_sitename(LEFT_SID);

		left_sitename = newCDKLabel(cdkscreen,
									width/2-strlen(msg[0]),
									height - 4,
									msg,
									1,
									FALSE,
									FALSE);
		if (!left_sitename) goto fail;

		msg[0] = engine_get_sitename(RIGHT_SID);

		right_sitename = newCDKLabel(cdkscreen,
									 width/2+2,
									 height - 4,
									 msg,
									 1,
									 FALSE,
									 FALSE);
		if (!right_sitename) goto fail;

	}



	display_details(&l_details, NULL);
	display_details(&r_details, NULL);

	
	if (engine_isconnected(LEFT_SID))
		my_setCDKFocusCurrent(cdkscreen, ObjPtr(l_files));
	else if (engine_isconnected(RIGHT_SID))
		my_setCDKFocusCurrent(cdkscreen, ObjPtr(r_files));
	else
		my_setCDKFocusCurrent(cdkscreen, ObjPtr(l_path));
	
	refreshCDKScreen(cdkscreen);

	return;

 fail:
	display_undraw();
	return;
}



void display_undraw()
{
	
	display_enable_focus();

	if (l_files) {
		destroyCDKSelection(l_files);
		l_files = NULL;
	}

	if (r_files) {
		destroyCDKSelection(r_files);
		r_files = NULL;
	}

	if (l_path) {
		destroyCDKEntry(l_path);
		l_path = NULL;
	}

	if (r_path) {
		destroyCDKEntry(r_path);
		r_path = NULL;
	}

	if (l_details) {
		destroyCDKLabel(l_details);
		l_details = NULL;
	}

	if (r_details) {
		destroyCDKLabel(r_details);
		r_details = NULL;
	}

	if (l_sortby) {
		destroyCDKItemlist(l_sortby);
		l_sortby = NULL;
	}

	if (r_sortby) {
		destroyCDKItemlist(r_sortby);
		r_sortby = NULL;
	}

	if (left_sitename) {
		destroyCDKLabel(left_sitename);
		left_sitename = NULL;
	}

	if (right_sitename) {
		destroyCDKLabel(right_sitename);
		right_sitename = NULL;
	}


	display_clear_lfiles();
	display_clear_rfiles();

	SAFE_FREE(l_the_files);
	l_num_allocated = 0;

	SAFE_FREE(r_the_files);
	r_num_allocated = 0;

}


void display_poll(void)
{

	if (l_clear) {
		l_clear = 0;
		display_clear_lfiles();
		display_set_lfiles(NULL);
	}

	if (r_clear) {
		r_clear = 0;
		display_clear_rfiles();
		display_set_rfiles(NULL);
	}

	switch(display_done) {
		
	case 1:
		main_go();
		break;

	}

	display_done = 0;

}




void display_disable_focus(void)
{

	if (l_path)
		ObjOf(l_path)->acceptsFocus    = FALSE;
	if (l_files)
		ObjOf(l_files)->acceptsFocus   = FALSE;
	if (l_details)
		ObjOf(l_details)->acceptsFocus = FALSE;
	if (l_sortby)
		ObjOf(l_sortby)->acceptsFocus  = FALSE;
	if (r_path)
		ObjOf(r_path)->acceptsFocus    = FALSE;
	if (r_files)
		ObjOf(r_files)->acceptsFocus   = FALSE;
	if (r_details)
		ObjOf(r_details)->acceptsFocus = FALSE;
	if (r_sortby)
		ObjOf(r_sortby)->acceptsFocus  = FALSE;
	
	focus_disabled = 1;
}

void display_enable_focus(void)
{

	if (l_path)
		ObjOf(l_path)->acceptsFocus    = TRUE;
	if (l_files)
		ObjOf(l_files)->acceptsFocus   = TRUE;
	if (l_details)
		ObjOf(l_details)->acceptsFocus = TRUE;
	if (l_sortby)
		ObjOf(l_sortby)->acceptsFocus  = TRUE;
	if (r_path)
		ObjOf(r_path)->acceptsFocus    = TRUE;
	if (r_files)
		ObjOf(r_files)->acceptsFocus   = TRUE;
	if (r_details)
		ObjOf(r_details)->acceptsFocus = TRUE;
	if (r_sortby)
		ObjOf(r_sortby)->acceptsFocus  = TRUE;

	my_setCDKFocusCurrent(cdkscreen, ObjPtr(l_path));

	focus_disabled = 0;
}
