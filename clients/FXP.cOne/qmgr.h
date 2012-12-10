#ifndef QMGR_H_INCLUDED
#define QMGR_H_INCLUDED







int          qmgr_checkclick        ( MEVENT * );
void         qmgr_draw              ( void );
void         qmgr_undraw            ( void );
void         qmgr_poll              ( void );

void         qmgr_qc                ( char **, char **, int );
void         qmgr_qs                ( char **, char **, int );
void         qmgr_get_queue         ( void );
int          qmgr_printf            (char const *fmt, ...);


#endif

