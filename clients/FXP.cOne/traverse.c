#include <cdk/cdk.h>

/**
 ** This source file was lifted from cdk-5.0-20050424
 **
 ** By Thomas Dickey etc.
 **
 ** Minor hacks by Jorgen Lundman.
 **
 **/

/* Start verbatim copy, due to static function definitions */

#define limitFocusIndex(screen, value) \
        (((value) >= (screen)->objectCount || (value) < 0) \
         ? 0 \
         : (value))

static int getFocusIndex (CDKSCREEN *screen)
{  
	int result = limitFocusIndex (screen, screen->objectFocus);

	return result;
}

static void setFocusIndex (CDKSCREEN *screen, int value)
{
	screen->objectFocus = limitFocusIndex (screen, value);
}

static void unsetFocus (CDKOBJS *obj)
{
	curs_set (0);
	if (obj != 0)
		{
			HasFocusObj (obj) = FALSE;
			UnfocusObj (obj);
		}
}

static void setFocus (CDKOBJS *obj)
{
	if (obj != 0)
		{
			HasFocusObj (obj) = TRUE;
			FocusObj (obj);
		}
	curs_set (1);
}

static CDKOBJS *switchFocus (CDKOBJS *newobj, CDKOBJS *oldobj)
{
	if (oldobj != newobj)
		{
			unsetFocus (oldobj);
			setFocus (newobj);
		}

	return newobj;
}
static CDKOBJS *handleMenu (CDKSCREEN *screen, CDKOBJS *menu, CDKOBJS *oldobj)
{
	bool done = FALSE;
	CDKOBJS *newobj;

	switchFocus (menu, oldobj);
	while (!done)
		{
			int key = getcCDKObject (menu);

			switch (key)
				{
				case KEY_TAB:
					{
						done = TRUE;
						break;
					}
				case KEY_ESC:
					{
						/* cleanup the menu */
						injectCDKMenu ((CDKMENU *) menu, key);
						done = TRUE;
						break;
					}
				default:
					{
						int m = injectCDKMenu ((CDKMENU *) menu, key);
						done = (m >= 0);
						break;
					}
				}
		}
	if ((newobj = getCDKFocusCurrent (screen)) == 0)
		newobj = setCDKFocusNext (screen);

	return switchFocus (newobj, menu);
}

/*
 * Save data in widgets on a screen
 */
static void saveDataCDKScreen (CDKSCREEN *screen)
{
	int i;

	for (i = 0; i < screen->objectCount; ++i)
		SaveDataObj (screen->object[i]);
}

/*
 * Refresh data in widgets on a screen
 */
static void refreshDataCDKScreen (CDKSCREEN *screen)
{
	int i;

	for (i = 0; i < screen->objectCount; ++i)
		RefreshDataObj (screen->object[i]);
}


/* End verbatim copy */


//
// If we change the current focus, the while loop in traverse need
// to notice this and change its "curobj".
// We iterate Next() until its the object we want. Lame? Maybe.. there
// are no support functions to convert obj* to index.
//
CDKOBJS *my_setCDKFocusCurrent(CDKSCREEN *screen, CDKOBJS *newobj)
{
	CDKOBJS *result = 0;
	CDKOBJS *curobj;
	int n = getFocusIndex (screen);
	int first = n;
   
   for (;;)
   {  
      if (++n >= screen->objectCount)
         n = 0;

      curobj = screen->object[n];
      if (curobj == newobj)
      {  
         result = curobj;
         break;
      }
      else
      {  
         if (n == first)
         {  
            break;
         }
      }
   }
   
   setFocusIndex (screen, (result != 0) ? n : -1);
   return result;
}








int my_traverseCDKScreen (CDKSCREEN *screen)
{
	CDKOBJS *curobj = NULL;

	// Get the focus
	curobj = getCDKFocusCurrent(screen);

	// If we didn't have one, or it was invalid, pick first one.
	if (!curobj)
		curobj = switchFocus (setCDKFocusNext (screen), 0);

	// Still no focus, bail.
	if (curobj == 0)
		return 0;

	refreshDataCDKScreen (screen);

	screen->exitStatus = CDKSCREEN_NOEXIT;



	while ((curobj != 0) && (screen->exitStatus == CDKSCREEN_NOEXIT))
		{
			int key;

			
			/* I want ASYNC, but for some reason you can't set this
			 * globally */
			// PreProcess can change Focus, we need to detect this
			if (getCDKFocusCurrent(screen) != curobj) {
				curobj = getCDKFocusCurrent(screen);
				if (!curobj) break;
			}
			
			wtimeout(curobj->inputWindow, 350);
			key = getcCDKObject (curobj);

			switch (key)
				{
				case KEY_BTAB:
					{
						curobj = switchFocus (setCDKFocusPrevious (screen), curobj);
						break;
					}
				case KEY_TAB:
					{
						curobj = switchFocus (setCDKFocusNext (screen), curobj);
						break;
					}
				case KEY_F(10):
					{
						/* save data and exit */
						exitOKCDKScreen (screen);
						break;
					}
				case CTRL('X'):
					{
						exitCancelCDKScreen (screen);
						break;
					}
				case CTRL('R'):
					{
						/* reset data to defaults */
						resetCDKScreen (screen);
						setFocus (curobj);
						break;
					}
				case CDK_REFRESH:
					{
						/* redraw screen */
						refreshCDKScreen (screen);
						setFocus (curobj);
						break;
					}
				case KEY_ESC:
					{
						/* find and enable drop down menu */
						int j;

						for (j = 0; j < screen->objectCount; ++j)
							if (ObjTypeOf (screen->object[j]) == vMENU)
								{
									curobj = handleMenu (screen, screen->object[j], curobj);
									break;
								}
						break;
					}


					/* wgetch() returns ERR if we timedout */
				case ERR:
					return -1;
					break;

				default:
					{
						InjectObj (curobj, key);
						break;
					}
				}

		}

	if (screen->exitStatus == CDKSCREEN_EXITOK)
		{
			saveDataCDKScreen (screen);
			return 1;
		}
	else
		return 0;
}


