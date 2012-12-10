// $Id: parser.h,v 1.2 2006/02/24 06:57:56 lundman Exp $
// Generic parser / function caller.
// Jorgen Lundman Febuary 1st, 2000

//
// Example definition of a parser_command_list:
//
// parser_command_list_t services_command_list[] = {
//   { COMMAND( "@ADDSERVICE" ), cmdservices_addservice },
//   { COMMAND( "@RMSERVICE"  ), cmdservices_rmservice  },
//   { COMMAND( "@SUBSCRIBE"  ), cmdservices_subscribe  },
//   { NULL,                  0, cmdservices_default    }
//
// Where each function (addservice,rmservice,subscribe) is called if
// a command matching it could be parsed, or, if nothing could be parsed
// the 'default' function is called. (The one with a NULL match).
//
// Each of these functions should be defined as:
// void cmdservices_addservice(char **keys, char **values, 
//                             int items,void *optarg);
//
//
// Default function is somewhat special case. Items will generally
// always be one of:
//   1 - keys[0] contains the entire un-parsed string
//   2 - values[0] contain the receiptient username, keys[1] the string. ????
//

/* Defines */

#define MAX_PAIRS  1000
#define DATA_BUFSZ 8192 // Used by buildpacket as maximum string

// Define string command and strlen at the same time, for more
// efficient comparison.
#define COMMAND( cmd ) (cmd) , sizeof (cmd) -1






/* Variables */

struct parser_command_list_s {
  char *command;
  int   strlen;
  void (*function)(char **, char **, int, void *);
};

typedef struct parser_command_list_s parser_command_list_t;


/* Functions */

int  parser_command(parser_command_list_t *, char *, void *);
void parser_keypairs(char *, char **, char **, int *, char *);
char *parser_findkey (char **, char **, int , char *);
char *parser_findkey_once (char **, char **, int , char *);
char *parser_buildstring(char **, char **, int);

