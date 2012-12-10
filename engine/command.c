// $Id: command.c,v 1.7 2011/12/05 07:52:55 lundman Exp $
//
// Jorgen Lundman 18th December 2003.
//
// Command Channel Functions. (GUI's talking to engine)
//
// This takes new connections on the command socket, and once they are
// authenticated, passes the connection to command2.c
//
#include <stdio.h>

#include "lion.h"

#include "debug.h"
#include "command.h"
#include "command2.h"
#include "settings.h"
#include "parser.h"
#include "engine.h"
#include "users.h"

#include "version.h"

//
// Open the listening port, and await connections.
// Upon connection (check SSL only) have client authenticate.
// Then pass onto command2.
//

static lion_t *command_listen = NULL;


static parser_command_list_t command_anonymous_list[] = {
	{ COMMAND( "QUIT" ),     command_cmd_quit },
	{ COMMAND( "SSL"  ),     command_cmd_ssl  },
	{ COMMAND( "AUTH" ),     command_cmd_auth },
	{ COMMAND( "HELP" ),     command_cmd_help },
	{ NULL,  0         ,                 NULL }
};


// This is an internal value to LION, you can check if the certificate was
// read such that we can be in server mode.
extern int net_server_SSL;



void command_init(void)
{

	if (command_listen)
		command_free();

	command_listen = lion_listen(&settings_values.command_port,
								 settings_values.command_iface,
								 LION_FLAG_EXCLUSIVE,
								 NULL);

	if (!command_listen) {

		perror("[command]");
		debugf("[command] listening socket failed to open: \n");

		engine_running = 0;

		return;
	}


	//  fulfill means we can just assume it will not be NULL
	lion_set_handler(command_listen, command_handler);

	debugf("[command] listening on port %u\n",
		   settings_values.command_port);

}


void command_free(void)
{

	if (!command_listen) return;

	lion_close(command_listen);

	command_listen = NULL;

}





//
// The main handler for the listening command channel, as well as
// "anonymous" connections. Once they have authenticated, we move them
// to a new handler.
//
int command_handler(lion_t *handle, void *user_data,
					int status, int size, char *line)
{
	unsigned long host;
	int port;


	switch(status) {

	case LION_CONNECTION_NEW:
		lion_set_handler(
						 lion_accept(
									 handle, 0, LION_FLAG_FULFILL,
									 NULL, NULL, NULL),
						 command_handler);
		break;

	case LION_CONNECTION_CONNECTED:

		lion_getpeername(handle, &host, &port);
		debugf("[command] New connection from %s:%u\n",
			   lion_ntoa(host), port);

		// Send greeting
		lion_printf(handle, "WELCOME|name=FXP.Oned|version=%d.%d|build=%d|protocol=%d.%d|SSL=%s\r\n",
					VERSION_MAJOR,
					VERSION_MINOR,
					VERSION_BUILD,
					PROTOCOL_MAJOR,
					PROTOCOL_MINOR,
					(settings_values.command_ssl_only == YNA_YES) &&
					net_server_SSL ? "forced" :
					net_server_SSL ? "optional" : "disabled" );

		break;


	case LION_CONNECTION_LOST:
		if (handle == command_listen) {
			debugf("[command] listening socket lost? This should not happen: %d:%s\n",
				   size, line ? line : "(null)");
			break;
		}

		/* FALL THROUGH */
	case LION_CONNECTION_CLOSED:

		lion_getpeername(handle, &host, &port);

		if (engine_running)
			debugf("[command] connection %s %s:%u %d:%s\n",
				   "closed",
				   lion_ntoa(host), port,
				   size, line ? line : "(null)");

		break;


	case LION_CONNECTION_SECURE_ENABLED:
		debugf("[command] connection upgraded to secure\n");
		break;

	case LION_CONNECTION_SECURE_FAILED:
		debugf("[command] connection failed to SSL\n");
		break;


	case LION_INPUT:
		debugf("[command] anonymous input '%s' (%d)\n",
               line,
               size);

		parser_command(command_anonymous_list, line, (void *)handle);
		break;


	}

	return 0;

}


















//
// PARSER COMMAND CALLBACKS
//
void command_cmd_quit(char **keys, char **values,
				int items,void *optarg)
{
	lion_t *handle = optarg;

	if (!handle) return;

	lion_printf(handle, "GOODBYE\r\n");
	lion_close(handle);

}


void command_cmd_auth(char **keys, char **values,
				int items,void *optarg)
{
	lion_t *handle = optarg;
	char *name, *pass;

	if (!handle) return;


	if ((settings_values.command_ssl_only == YNA_YES) &&
		(!lion_ssl_enabled(handle))) {

		// Kinda sucks, if they don't read the SSL status in the greeting
		// and attempt to authenticate, they have now exposed the user and
		// password in plain text.
		lion_printf(handle,
					"AUTH|CODE=1503|MSG=Secure channel enforced.\r\n");
		return;

	}



	name = parser_findkey(keys, values, items, "USER");
	pass = parser_findkey(keys, values, items, "PASS");

	if (!name || !pass) {
		lion_printf(handle,
					"AUTH|CODE=1501|MSG=Required arguments: USER PASS\r\n");
		return;
	}


	// Authenticate
	if (!users_authenticate(name, pass)) {
		lion_printf(handle, "AUTH|CODE=1502|MSG=Login incorrect\r\n");
		return;
	}


	// Successful. SSL only?
	lion_printf(handle, "AUTH|CODE=0|MSG=Successful\r\n");


	command2_register(handle, name);

}


void command_cmd_help(char **keys, char **values,
					  int items,void *optarg)
{
	lion_t *handle = optarg;
	int i;

	if (!handle) return;

    debugf("[command] help\n");

	lion_printf(handle, "HELP|CODE=0|Msg=");

	for (i = 0;
		 command_anonymous_list[i].command;
		 i++) {

		lion_printf(handle, " %s",
					command_anonymous_list[i].command);

	}

	lion_printf(handle, ".\r\n");

}



void command_cmd_ssl(char **keys, char **values,
					  int items,void *optarg)
{
	lion_t *handle = optarg;

	if (!handle) return;

	// Attempt SSL
	if (lion_ssl_set(handle, LION_SSL_SERVER) == 1)
		lion_printf(handle,
					"SSL|CODE=0|Msg=Attempting SSL negotiations.\r\n");
	else
		lion_printf(handle,
					"SSL|CODE=1504|Msg=SSL support not enabled.\r\n");

}


