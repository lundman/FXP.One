#ifndef CONNECT_H_INCLUDED
#define CONNECT_H_INCLUDED


void    draw_connect                        ( void );
void    undraw_connect                      ( void );
int     connect_process                     ( void );
int     connect_checkclick                  ( MEVENT *event);
void    connect_poll                        ( void );
void    connect_draw_inputs                 ( void );


#endif
