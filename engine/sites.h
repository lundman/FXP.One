// $Id: sites.h,v 1.11 2010/10/12 02:36:32 lundman Exp $
//
// Jorgen Lundman 30th January 2004.

#ifndef SITES_H_INCLUDED
#define SITES_H_INCLUDED

#include "settings.h"

// Defines.

#define SITES_LIST_ALL_SHORT  -3
#define SITES_LIST_SAVE -2
#define SITES_LIST_ALL  -1


#define SITES_CONF ".FXP.One.sites"
#define DEFAULT_LISTARGS "-lL"


// Variables

typedef struct sites_struct {

	int id;              // id assigned at load time.

	char *name;

	char *host;
	unsigned long ip;    // assigned from host.
	unsigned int port;

	unsigned long iface;  // optional bind interface and ports.
	unsigned int iport;

	char *user;
	char *pass;

	yesnoauto_t passive;  // yes -> user pasv, no -> use port, auto -> try pasv
	                      // then port.

	yesnoauto_t fxp_passive; // yes -> ftpd can only take pasv
	                         // no  -> ftpd can only take port
	                         // auto-> ftpd can take either (we decide).

	yesnoauto_t control_TLS; // auto-> attempt TLS, but downgrade to plain
	                         // yes -> attempt TLS, but fail if not available.
	                         // no  -> just use plain.

	yesnoauto_t data_TLS;    // auto-> attempt TLS, but downgrade to plain
	                         // downgrade for FXP sessions anyway.
	                         // yes -> attempt TLS, but fail if not available.
	                         // no  -> just use plain.

	yesnoauto_t desired_type;   // NO=ascii, YES=binary, AUTO=change as needed.

	yesnoauto_t resume;         // YES/AUTO=try resume first. NO=overwrite

	yesnoauto_t resume_last;    // NO/AUTO=resume now, YES=re-queue to last

	yesnoauto_t pret;        // NO=don't PRET, YES=send PRET, AUTO=if site has
	                         // feature PRET set we send.

	yesnoauto_t use_stat;	// yes/auto -> uses 'stat -al' for dirlisting instead of 'list'

	char *file_skiplist;     // slash seperated skip list
	char *directory_skiplist;

	char *file_passlist;     // opposite to skiplist. Usually empty(=*)
	char *directory_passlist;

	char *file_movefirst;    // slash seperated patterns of items to queue top
	char *directory_movefirst;

	yesnoauto_t file_skipempty; // YES/AUTO skip empty files
	yesnoauto_t directory_skipempty;

	// Any optional field (not required, or processed into this structire)
	// are stored here.
	int items;
	char **keys;
	char **values;


	struct sites_struct *next;

} sites_t;

/*
	// Internals, only during operation
	int current_type;           // ascii=NO/0, binary=YES/1
	int current_prot;           // clear=NO/0, private=YES/1

*/



// Functions

void      sites_init        ( void );
void      sites_free        ( void );
int       sites_handler     ( lion_t *, void *, int, int, char * );
void      sites_conf        ( char **, char **, int ,void * );
void      sites_save        ( void );
int       sites_iodone      ( void );
void      sites_del         ( sites_t * );
void      sites_setkey      ( char * );

sites_t  *sites_newnode     ( void );
void      sites_insertnode  ( sites_t * );
void      sites_removenode  ( sites_t * );
void      sites_freenode    ( sites_t * );

int       sites_parsekeys   ( char **, char **, int, sites_t * );
void      sites_cmd_site    ( char **, char **,	int, void * );
void      sites_listentry   ( lion_t *, sites_t *, int );
void      sites_listentries ( lion_t *, int );
sites_t  *sites_getid       ( int );
char     *sites_find_extra  ( sites_t *site, char *key);
char     *site_get_listargs ( sites_t *site );

#endif
