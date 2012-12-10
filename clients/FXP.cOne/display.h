#ifndef DISPLAY_H_INCLUDED
#define DISPLAY_H_INCLUDED

#include "file.h"


enum sortby_enum {
	SORT_NAME = 0,
	SORT_RNAME,
	SORT_SIZE,
	SORT_RSIZE,
	SORT_DATE,
	SORT_RDATE,
	SORT_USER,
	SORT_RUSER,
	SORT_GROUP,
	SORT_RGROUP
};

typedef enum sortby_enum sortby_t;

#define DISPLAY_ALL       0
#define DISPLAY_ONLYFILES 1
#define DISPLAY_ONLYDIRS  2




int          display_checkclick        ( MEVENT * );
void         display_draw              ( void );
void         display_undraw            ( void );
void         display_poll              ( void );
void         display_queue_selected    ( int );
void         display_queued_item       ( int, unsigned int );
void         display_sort_files        ( int, int );
void         display_inject_file       ( int, file_t * );
void         display_clear_selected    ( int );
void         display_set_lpath         ( char * );
void         display_set_rpath         ( char * );
void         display_set_files         ( int, char * );
void         display_select_fids       ( int , char * );
void         display_disable_focus     ( void );
void         display_enable_focus      ( void );

void         display_iterate_selected  ( int, int (*callback)(char *), int);
void         display_refresh           ( int );





#endif

