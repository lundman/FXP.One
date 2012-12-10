#ifndef COMMAND2_H_INCLUDED
#define COMMAND2_H_INCLUDED


// Defines..




// Variables..

struct command2_node_struct {

	char *name;
	lion_t *handle;

	unsigned long host;
	int port;

};

typedef struct command2_node_struct command2_node_t;



// Functions..

int       command2_handler           ( lion_t *, void *, int, int, char * );
void      command2_register          ( lion_t *, char * );


void      command2_cmd_quit          ( char **, char **, int , void * );
void      command2_cmd_help          ( char **, char **, int , void * );
void      command2_cmd_who           ( char **, char **, int , void * );
void      command2_cmd_shutdown      ( char **, char **, int , void * );
void      command2_cmd_setpass       ( char **, char **, int , void * );

void      command2_cmd_siteadd       ( char **, char **, int , void * );

void      command2_cmd_sitelist      ( char **, char **, int , void * );
void      command2_cmd_sitemod       ( char **, char **, int , void * );
void      command2_cmd_sitedel       ( char **, char **, int , void * );

void      command2_cmd_sessionnew    ( char **, char **, int , void * );
void      command2_cmd_sessionfree   ( char **, char **, int , void * );

void      command2_cmd_dirlist       ( char **, char **, int , void * );


void      command2_cmd_queuenew      ( char **, char **, int , void * );
void      command2_cmd_qadd          ( char **, char **, int , void * );
void      command2_cmd_qlist         ( char **, char **, int , void * );
void      command2_cmd_qget          ( char **, char **, int , void * );
void      command2_cmd_qerr          ( char **, char **, int , void * );
void      command2_cmd_qgrab         ( char **, char **, int , void * );
void      command2_cmd_qdel          ( char **, char **, int , void * );
void      command2_cmd_qmove         ( char **, char **, int , void * );
void      command2_cmd_qcompare      ( char **, char **, int , void * );
void      command2_cmd_queuefree     ( char **, char **, int , void * );
void      command2_cmd_qclone        ( char **, char **, int , void * );

void      command2_cmd_go            ( char **, char **, int , void * );
void      command2_cmd_stop          ( char **, char **, int , void * );



void      command2_cmd_quote         ( char **, char **, int , void * );
void      command2_cmd_cwd           ( char **, char **, int , void * );
void      command2_cmd_pwd           ( char **, char **, int , void * );
void      command2_cmd_size          ( char **, char **, int , void * );

void      command2_cmd_dele          ( char **, char **, int , void * );
void      command2_cmd_mkd           ( char **, char **, int , void * );
void      command2_cmd_rmd           ( char **, char **, int , void * );
void      command2_cmd_site          ( char **, char **, int , void * );
void      command2_cmd_ren           ( char **, char **, int , void * );
void      command2_cmd_mdtm          ( char **, char **, int , void * );

void      command2_cmd_subscribe     ( char **, char **, int , void * );
void      command2_cmd_unsubscribe   ( char **, char **, int , void * );

#endif
