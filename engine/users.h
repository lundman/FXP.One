// $Id: users.h,v 1.3 2007/01/17 08:55:19 lundman Exp $
// 
// Jorgen Lundman 10th October 2003.

#ifndef USERS_H_INCLUDED
#define USERS_H_INCLUDED

// Defines.






// Variables

typedef struct users_struct {

	char *name;
	char *pass;

	struct users_struct *next;

} users_t;






// Functions


void      users_init        ( void );
void      users_free        ( void );
int       users_handler     ( lion_t *, void *, int, int, char * );
void      users_conf        ( char **, char **, int ,void * );
void      users_save        ( void );
int       users_iodone      ( void );
void      users_setkey      ( char * );
int       users_authenticate( char *, char * );
int       users_setpass     ( char *, char * );

// parser commands
void      users_add         ( char **, char **, int , void * );
void      users_mute        ( char **, char **, int , void * );


#endif
