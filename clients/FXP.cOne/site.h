#ifndef SITE_H_INCLUDED
#define SITE_H_INCLUDED

#define SITE_CMD     1
#define SITE_DELE    2
#define SITE_MKD     4
#define SITE_RENAME  5



void    site_draw                        ( void );
void    site_undraw                      ( void );
int     site_process                     ( void );
int     site_checkclick                  ( MEVENT *event);
void    site_poll                        ( void );
void    site_cmd                         ( int );
void    site_dele                        ( int );
void    site_mkd                         ( int );
void    site_rename                      ( int );


#endif
