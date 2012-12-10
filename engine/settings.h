// $Id: settings.h,v 1.5 2012/02/29 06:44:49 lundman Exp $
//
// Jorgen Lundman 10th October 2003.

#ifndef SETTINGS_H_INCLUDED
#define SETTINGS_H_INCLUDED

#include "lion.h"

// Defines.
#define CERTFILE "lion.pem"

enum yesnoauto_enum {
  YNA_NO = 0,
  YNA_YES,
  YNA_AUTO
};

typedef enum yesnoauto_enum yesnoauto_t;





// Variables

typedef struct settings_struct {

	int command_port;
	unsigned long command_iface;

	yesnoauto_t command_ssl_only;

	int http_port;
	unsigned long http_iface;
	yesnoauto_t http_ssl_only;

} settings_t;

extern settings_t settings_values;


extern int settings_initialising;



// Functions


void      settings_init        ( void );
int       settings_handler     ( lion_t *, void *, int, int, char * );
void      settings_conf        ( char **, char **, int ,void * );
void      settings_save        ( void );
void      settings_defaults    ( void );
yesnoauto_t str2yna            ( char * );


#endif
