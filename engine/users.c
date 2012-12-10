// $Id: users.c,v 1.6 2012/02/29 06:44:49 lundman Exp $
//
// Jorgen Lundman 14th October 2003.
//
// Server Wide settings, read off disk.
//


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <openssl/blowfish.h>

#include "lion.h"
#include "misc.h"

#include "debug.h"
#include "users.h"
#include "parser.h"




static parser_command_list_t users_command_list[] = {
	{ COMMAND( "USER" ),     users_add },
	{ COMMAND( "MUTE" ),    users_mute },
	{ NULL,  0         ,          NULL }
};


static int users_mute_warning = 0;
static char *users_key = NULL;


// The linked list of users. These are the on-disk ones, not the logged in.
static users_t *users_head = NULL;
static lion_t *users_file = NULL;




void users_setkey(char *key)
{

	if (key && !*key)
		key = NULL;

	SAFE_COPY(users_key, key);

}




void users_init(void)
{

	// Read the users file into memory. This can be issued many times
	// to re-read it if there are changes.

	users_mute_warning = 0;


	// Using lion to read a config file, well, well...

	users_file = lion_open(".FXP.One.users",
						   O_RDONLY,
						   0600,
						   LION_FLAG_NONE, NULL);


	if (users_file) {

		lion_set_handler(users_file, users_handler);

		if (users_key) {

			lion_ssl_set(users_file, LION_SSL_FILE);

			lion_ssl_setkey(users_file, users_key, strlen(users_key));

		}

	} else {

		// No users file, create a new one
		users_head = (users_t *) malloc(sizeof(*users_head));
		if (!users_head) return;

		memset(users_head, 0, sizeof(*users_head));

		users_head->name = strdup("admin");
		users_head->pass = strdup("admin");

		users_save();

        printf("\nCreated default login 'admin' with password 'admin'\n\n");

	}

}



void users_free(void)
{
	users_t *runner, *next;

	for (runner = users_head; runner; runner = next ) {

		next = runner->next;

		SAFE_FREE(runner->name);

		// zero out passwords
		memset(runner->pass, 0, strlen(runner->pass));
		SAFE_FREE(runner->pass);

		SAFE_FREE(runner);

	}

}





//
// userinput for reading the config.
//

int users_handler(lion_t *handle, void *user_data,
				  int status, int size, char *line)
{

	switch(status) {

	case LION_FILE_OPEN:
		debugf("users: reading users file...\n");
		break;


	case LION_FILE_FAILED:
	case LION_FILE_CLOSED:
		debugf("users: finished reading users.\n");

		users_file = NULL;

		if (!users_mute_warning) {
			printf("[users] WARNING - users file was not read successfully. Correct password?\n");
		}
		break;

	case LION_INPUT: // un-encrypted
		//debugf("users: read '%s'\n", line);

		parser_command(users_command_list, line, NULL);

		break;

	case LION_BINARY: // Shouldn't happen
		break;


	}

	return 0;

}




void users_save(void)
{

	// Read the users file into memory. This can be issued many times
	// to re-read it if there are changes.

	users_t *runner;

	// No warning for writing.
	users_mute_warning = 1;


	// Using lion to read a config file, well, well...

	users_file = lion_open(".FXP.One.users",
						   O_WRONLY|O_CREAT|O_TRUNC,
						   0600,
						   LION_FLAG_NONE, NULL);


	if (users_file) {

		lion_set_handler(users_file, users_handler);

		// We are writing, don't try to read the file.
		lion_disable_read(users_file);

		if (users_key) {

			lion_ssl_set(users_file, LION_SSL_FILE);

			lion_ssl_setkey(users_file, users_key, strlen(users_key));

		}

		// Write all users
		for (runner = users_head; runner; runner = runner->next ) {

			if (runner->name && runner->pass)
				lion_printf(users_file, "USER|NAME=%s|PASS=%s\r\n",
							runner->name,
							runner->pass);
		}

		// Write something to the file
		lion_printf(users_file, "MUTE|DESC=Mute warning to show we read users file ok.\r\n");
		lion_close(users_file);
		return;

	}

	printf("[users]: Warning, failed to create users file? Permissions? Disk full?\n");

}



int users_authenticate(char *name, char *pass)
{
	users_t *runner;

	for (runner = users_head; runner; runner = runner->next ) {

		if (!mystrccmp(runner->name, name) &&
			!strcmp(runner->pass, pass))
			return 1;

	}

	return 0;

}


int users_setpass(char *name, char *pass)
{
	users_t *runner;

	for (runner = users_head; runner; runner = runner->next ) {

		if (!mystrccmp(runner->name, name)) {
			SAFE_COPY(runner->pass, pass);
			users_save();
			return 1;
		}
	}

	return 0;

}






//
// Commands from parser
//

void users_add(char **keys, char **values,
			   int items,void *optarg)
{
	char *name, *pass;
	users_t *newd;

	name = parser_findkey(keys, values, items, "NAME");
	pass = parser_findkey(keys, values, items, "PASS");

	if (!name || !pass) {
		debugf("[users] corrupt user entry\n");
		return;
	}


	debugf("[users]: reading '%s'\n", name);

	newd = (users_t *) malloc(sizeof(*users_head));
	if (!newd) {
		debugf("[users] out of memory\n");
		return;
	}

	memset(newd, 0, sizeof(*newd));

	SAFE_COPY(newd->name, name);
	SAFE_COPY(newd->pass, pass);

	newd->next = users_head;
	users_head = newd;


}

void users_mute(char **keys, char **values,
				int items,void *optarg)
{

	debugf("[users] mute read\n");
	users_mute_warning = 1;

}

int users_iodone(void)
{

	if (users_file) return 0;

	return 1;
}
