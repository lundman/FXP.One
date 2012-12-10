// $Id: manager.h,v 1.13 2010/10/12 02:36:32 lundman Exp $
//
// Jorgen Lundman 18th March 2004.

#ifndef MANAGER_H_INCLUDED
#define MANAGER_H_INCLUDED

#include "sites.h"
#include "session.h"
#include "command2.h"

// Defines.

enum manager_user_flag_enum {
	MANAGER_USERFLAG_NONE = 0,
	MANAGER_USERFLAG_LOG,       // Send ALL ftpd messages to user.
	MANAGER_USERFLAG_RNFR_OK    // RNFR was successful, so reply at RNTO
};

typedef enum manager_user_flag_enum manager_user_flag_t;




// Variables

struct manager_struct {

	sites_t *site;         // current site, if any.
	session_t *session;    // current session, if any.
	session_t *old_session;

	unsigned int id;       // this nodes session id

	unsigned int connected;

	command2_node_t *user; // current user owner, if any.
	manager_user_flag_t user_flags;

	yesnoauto_t locked;    // locked by other manager for queue processing?
	                       // no   = user controls it.
	                       // yes  = remote manager locked
	                       // auto = locked by _this_ manager

	unsigned int qid;      // Queue-ID we may belong to.

    unsigned int is_dmovefirst;

	struct manager_struct *next;

};

typedef struct manager_struct manager_t;






// Functions

int        manager_init                 ( void );
void       manager_free                 ( void );


manager_t *manager_new                  ( command2_node_t *, sites_t *, int );
void       manager_release              ( manager_t * );

manager_t *manager_find_fromsession     ( session_t * );
manager_t *manager_find_fromsid         ( unsigned int );

void       manager_dirlist              ( manager_t *, char *, char *, int );


void       manager_quote                ( manager_t *, char *, int );
void       manager_cwd                  ( manager_t *, char *, int );
void       manager_pwd                  ( manager_t *, int );
void       manager_size                 ( manager_t *, char *, int );


void      *manager_queuenew             ( manager_t *, manager_t * );


void       manager_set_user             ( manager_t *, command2_node_t * );
void       manager_unlink_user          ( manager_t * );
void       manager_unlink_all_user      ( command2_node_t * );
void       manager_cleanup              ( void );
command2_node_t *manager_get_user       ( manager_t * );

void       manager_dele                 ( manager_t *, char *, int );
void       manager_mkd                  ( manager_t *, char *, int );
void       manager_rmd                  ( manager_t *, char *, int );
void       manager_site                 ( manager_t *, char *, int );
void       manager_ren                  ( manager_t *, char *, char *, int );
void       manager_mdtm                 ( manager_t *, char *, int );


#endif
