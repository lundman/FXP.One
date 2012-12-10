#ifndef ENGINE_H_INCLUDED
#define ENGINE_H_INCLUDED

#include "lion.h"

extern lion_t *engine_handle;

#define LEFT_SID  0
#define RIGHT_SID 1




void engine_connect ( char *host, int port, char *user, char *pass);
void engine_spawn   ( char *key, char *user, char *pass);
int  engine_init    ( void );
void engine_free    ( void );
void engine_poll    ( void );
void engine_sitelist( void (*callback)(char **, char **, int));
void engine_qget    ( void (*callback)(char **, char **, int));
void engine_qerr    ( void (*callback)(char **, char **, int));
void engine_qlist   ( void (*callback)(char **, char **, int));
void engine_qmove   ( int, int, void (*callback)(char **, char **, int));
void engine_qgrab   ( unsigned int, int, void (*callback)(int, char *));
void engine_qdel    ( int, void (*callback)(char **, char **, int));

void engine_site    ( int, int, char *, void (*callback)(char **, char **, int));
void engine_dele    ( int, int, char *, void (*callback)(char **, char **, int));
void engine_rmd     ( int, int, char *, void (*callback)(char **, char **, int));
void engine_mkd     ( int, int, char *, void (*callback)(char **, char **, int));
void engine_rename  ( int, int, char *, char *, void (*callback)(char **, char **, int));

int  engine_queue   ( int, unsigned int );
void engine_subscribe ( unsigned int );
void engine_queuefree ( unsigned int );

void engine_connect_left ( unsigned int, char * );
void engine_connect_right( unsigned int, char * );

int engine_hasqueue ( void );


void engine_pwd     ( int );
void engine_cwd     ( int , char * );
void engine_set_startdir ( int, char * );
char *engine_get_startdir( int );

int  engine_go      ( void );

int  engine_isconnected( int );

void engine_compare(void);
unsigned int engine_current_qid(void);
char *engine_get_sitename(int);
void engine_set_sitename(int, char *);


#endif
