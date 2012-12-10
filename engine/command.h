#ifndef COMMAND_H_INCLUDED
#define COMMAND_H_INCLUDED


// Defines..




// Variables..





// Functions..

void      command_init              ( void );
void      command_free              ( void );
int       command_handler           ( lion_t *, void *,	int, int, char * );


void      command_cmd_quit          ( char **, char **, int , void * );
void      command_cmd_auth          ( char **, char **, int , void * );
void      command_cmd_help          ( char **, char **, int , void * );
void      command_cmd_ssl           ( char **, char **, int , void * );


#endif
