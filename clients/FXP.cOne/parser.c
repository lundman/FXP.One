// $Id: parser.c,v 1.4 2005/08/12 06:25:03 lundman Exp $
// Generic parser / function caller.
// Jorgen Lundman Febuary 1st, 2000

#include <stdio.h>
#include <string.h>

#include "parser.h"
#include "misc.h"
//#include "sockets.h" // To get xfer buffer size

#ifdef WIN32
#define strcasecmp stricmp
#define strncasecmp strnicmp
#endif



//
// Parse input strings into forms we like.
//
// Input form is of the format: SAY|FROM=jim|MSG=hello there|TO=alex\r\n
// or:  fred:SAY|TO=roger|MSG=hello hello\r\n
//
// First we check if it starts with a known command and if so
// split the command pairs
//



int parser_command(parser_command_list_t *cmds, char *line, void *optarg)
{
  char *PC;      // Saved position into the string we are processing
  char *command;  // Holder for the current command issued
  char *from;     // Receipient holder
  int i;
  char *keys[MAX_PAIRS], *values[MAX_PAIRS];
  int items;

  PC = line;      // start at the beginning of the string.

  command = NULL;
  from    = NULL;


  misc_digtoken_optchar = 0;


  // Fetch the command (or possible from field)
  if (!(command = misc_digtoken( &PC, ":|\r\n")))
      goto parser_error;



  // Check if we matched with a ':' if so, it is the user field we just read
  if (misc_digtoken_optchar == ':') {
    
    from = command;

    // Ok fetch the real command
    if (!(command = misc_digtoken( &PC, ":|\r\n")))
      goto parser_error;

  }

  // Ok, we have "command" and possible "from"
  for (i = 0; cmds[i].command; i++) {

	  //if (!strcasecmp( command, cmds[i].command ) ) {
	  // We compare strncase with strlen+1, so that "play" don't match "player"
	  if (!strncasecmp( command, cmds[i].command, cmds[i].strlen + 1 ) ) {

      // Matched command, call the relevant function
      // but first parse and split up the argument pairs
      // keys[] and num_items, like argc/argv
      // values[] 

		  // printf("[Parser] Matched command '%s'\n", cmds[i].command);
      
      parser_keypairs( command, keys, values, &items, PC );

      cmds[i].function( keys, values, items, optarg );

      return 0;

    }

  }

  // If the NULL entry in the command list, have a non-null function
  // set, we call it with whatever we failed to parse.
#if 1
  if ( cmds[i].function ) {

	  //       printf("line '%s' PC '%s' command '%s'\n", line, PC, command);

	/*
    items     = 1;
    keys[0]   = command; // The input is the first key
    values[0] = from;    // and the from, if any, if the first value


    if (PC && *PC) {

      items++;
      keys[1] = PC;
      values[1] = NULL;

    }
	*/

      parser_keypairs( command, keys, values, &items, PC );
    
	  //   printf("[Parser] Calling default function\n");


    cmds[i].function( keys, values, items, optarg );
    return 0;
  }
#endif

  //printf("[Parser] No match, i is %d for %08x\n", i, cmds[i].function);


  // Unknown command - oh well
  // also, any parsing errors
 parser_error:

  //log_printf(LOG_DEBUG, "[Parser]: Unknown command or Parse error: '%s'(%s)\r\n", line, PC);

  return -1;

}


void parser_keypairs(char *command, char **keyv, char **valuev, int *items, char *line )
{
  int itemc;
  char *cp;
  char *tmp;

  for (cp = line, itemc = 0;  ; itemc++ ) {


    // Check for overflow
    if (itemc >= MAX_PAIRS) {
		printf("[Parser] Maximum key pairs hit (%d) ignoring trailing junk '%s'\n", MAX_PAIRS, cp);
      break;
    }



    tmp = misc_digtoken( &cp, "|\r\n" );

    if ( !tmp ) break;       // Last pair read, exit

    keyv[ itemc ] = tmp;     // This is the key-word

    // if there is an = assign it
    // if not, it's set to NULL
    if ( misc_digtoken( &tmp, "=" ) && *tmp) {
      valuev[ itemc ] = tmp;
    } else {
      valuev[ itemc ] = "";
    }
    //    printf("Set with '%s'\n", tmp ? tmp : "(null)");
    
  }

  // Add the type=command part if there's room.
  keyv[ itemc ] = "type";
  valuev[ itemc ] = command;
  itemc++;

  *items = itemc;



#if 0
  {
    int i;
    printf("[Parser] Done parsing, num items is %d and they are:\n", itemc);
    
    printf(" %10s = %10s \n", "key", "value");
    for (i = 0; i < itemc; i++)
      printf("'%10s'='%10s'\n", keyv[ i ], 
		 valuev[ i ] ? valuev[ i ] : "(null)");
    
  }
#endif

}



char *parser_findkey(char **keyv, char **valuev, int items, char *keyword )
{
  int i;

  for (i = 0; i < items; i++) {

    if (keyv[i] && !strcasecmp(keyword, keyv[i])) {

#if 0
		printf("[parser] found key '%s' value '%s'\n", keyv[i], valuev[i]);
#endif

      return valuev[i];

    }

  }

  return NULL;

}


char *parser_findkey_once(char **keyv, char **valuev, int items, char *keyword )
{
  int i;

  for (i = 0; i < items; i++) {

    if (keyv[i] && !strcasecmp(keyword, keyv[i])) {

#if 0
		printf("[parser] found key '%s' value '%s'\n", keyv[i], valuev[i]);
#endif

		keyv[i] = NULL;

      return valuev[i];

    }

  }

  return NULL;

}




char *parser_buildstring(char **keys, char **values, int items)
{
  static char result[DATA_BUFSZ];  // This is max xfer size as well
  int index, i;
  int keylen, valuelen;


  index     = 0;
  result[0] = 0;

  for (i = 0; i < items; i++) {

    if (i) {
      result[index++] = '|';
    }

	if (!keys[i] || !values[i]) continue;

    keylen   = strlen(   keys[ i ] );
    valuelen = strlen( values[ i ] );


    if ((index + keylen + valuelen + 3) >= DATA_BUFSZ) {
      // Possible buffer over run here. Truncate the string and stay alive
      // +3 is for '=' and '|' and \0
      break;
    }

    strcpy( &result[index], keys[ i ]);
    index += keylen;

    result[index++] = '=';

    // BUG 12/2/02 - Last pair could be empty, so this wouldn't be terminated.
    result[index]   = 0;

    if (values[i] && values[i][0]) {
      strcpy( &result[index], values[ i ]);
      index += valuelen;
    }

  }



  //  printf("[Parsers]: Returning string '%s'\n", result);

  return result;

}
