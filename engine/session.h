// $Id: session.h,v 1.10 2005/08/12 06:18:36 lundman Exp $
// 
// Jorgen Lundman 5th February 2004.

#ifndef SESSION_H_INCLUDED
#define SESSION_H_INCLUDED

#include "lion.h"
#include "sites.h"
#include "file.h"



// Defines.

#define SESSION_CMDQ_FLAG_LOG 1   // Send all lines with this command, not 
                                  // the final reply code line.
#define SESSION_CMDQ_FLAG_INTERNAL 2 // Send events internally.
#define SESSION_CMDQ_FLAG_RAWLIST  4 // Include RAW entry in dirlist.
#define SESSION_CMDQ_FLAG_CACHEOK  8 // cached dirlist is OK
#define SESSION_CMDQ_FLAG_FORCE   16 // force command refresh


#define SESSION_CMDQ_UNIT     5   // allocate cmdq's in chunks of 5.

enum session_events {
	SESSION_EVENT_IDLE = 0,
	SESSION_EVENT_LOG,
	SESSION_EVENT_CMD,
	SESSION_EVENT_LOST,
	SESSION_EVENT_DIRLIST
};

typedef enum session_events session_event_t;



#include "session_states.h"


// Features are capabilities that the FTPD HAS
#define FEATURE_SSL  1    // Is this session using SSL?
#define FEATURE_SSCN 2    // Does this site have SSCN capabilities?
#define FEATURE_CCSN 4    // Does this site have CCSN capabilities?
#define FEATURE_SIZE 8    // Does this site have SIZE command?
#define FEATURE_PRET 16   // Does this site have PRET capabilities?
#define FEATURE_XDUPE 32  // Does this site have X-DUPE capabilities?


// Status is toggles for each state we are currently in.
#define STATUS_ON_SSL      1
#define STATUS_ON_BINARY   2
#define STATUS_ON_SSCN     4
#define STATUS_ON_CCSN     8
#define STATUS_ON_PRIVACY 16
#define STATUS_ON_SIZE    32 // This is set when we know site has SIZE cmd.
#define STATUS_ON_REST    64 // This is set when last REST was non-zero
#define STATUS_ON_XDUPE  128 // This is set if "Enabled" was sent with XDUPE






// Variables


struct cmdq_struct {

	char *cmd;       // The string to send/have sent.
	int id;          // caller's ID associated with this cmd.
	int subid;       // caller's ID associated with this cmd.
	int flags;       // any flags associated

};

typedef struct cmdq_struct cmdq_t;


struct session_struct {

	lion_t *handle;

	lion_t *data_handle;

	sites_t *site;

	void (*handler)(struct session_struct *, int, int, int, char *);

	// The command queue
	int cmdq_allocated;  // allocated in units of SESSION_CMDQ_UNIT
	int cmdq_size;       // current # of items to process.
	cmdq_t *cmdq;

	char *pwd;           // site's CWD
	char *saved_cwd;     // saving wanted CWD until outcome is known.

	session_state_t state; // current state of this session.

	int isidle;
	int forcebusy;  // if we are busy - pause cmdq

	int flags;

	unsigned long data_host;
	int data_port;


	unsigned int features;
	unsigned int status; 

	// File list holders for dirlist.
#define SESSION_FILES_INCREMENT 50
	file_t **files;
	unsigned int files_allocated;  // size of memory area for ptrs.
	unsigned int files_used;       // number ptrs actually used.

	int files_reply_id;            // id to send user when done.


	// Do we want next pointers? Not really.
};

typedef struct session_struct session_t;


typedef void (*session_handler_t)(struct session_struct *, 
								  int, int, int, char *);


// Functions

session_t *session_new           ( session_handler_t, sites_t * );
void       session_free          ( session_t * );
session_handler_t session_set_handler ( session_t *, session_handler_t );
int        session_isidle        ( session_t * );

void       session_state_process ( session_t *, int, char * );
void       session_set_state     ( session_t *, session_state_t, int );
int        session_handler       ( lion_t *, void *, int , int , char * );
void       session_cmdq_reply    ( session_t *, int , char * );
void       session_cmdq_new      ( session_t *, int , int , char const * );
void       session_cmdq_newf     ( session_t *, int , int , char const *, ... );
void       session_setclose      ( session_t *, char * );
void       session_dirlist       ( session_t *, char *, int, int );
void       session_cmdq_restart  ( session_t * );
void       session_release_files ( session_t * );
void       session_parse         ( session_t *, char * );
void       session_abor          ( session_t * );

file_t    *session_getfid        ( session_t *, unsigned int );
file_t    *session_getname       ( session_t *, char * );
 
unsigned int session_feat        ( session_t * );


void       session_type          ( session_t *, int, int, unsigned int );
void       session_ccsn          ( session_t *, int, int, unsigned int );
void       session_sscn          ( session_t *, int, int, unsigned int );
void       session_prot          ( session_t *, int, int, unsigned int );
void       session_cwd           ( session_t *, int, int, char * );
void       session_size          ( session_t *, int, int, char * );
void       session_rest          ( session_t *, int, int, lion64_t );
void       session_stor          ( session_t *, int, int, int, char *);
void       session_retr          ( session_t *, int, int, int, char *);

char      *session_pwd_reply     ( session_t *, int, char *);
void       session_send_xdupe    ( session_t * );


#endif
