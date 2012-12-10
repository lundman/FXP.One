
/*- 
 * 
 * New BSD License 2006
 *
 * Copyright (c) 2006, Jorgen Lundman
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 * 1 Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * 2 Redistributions in binary form must reproduce the above
 *   copyright notice, this list of conditions and the following
 *   disclaimer in the documentation and/or other materials provided
 *   with the distribution.  
 * 3 Neither the name of the stuff nor the names of its contributors 
 *   may be used to endorse or promote products derived from this
 *   software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

/*
 * getdirs example
 *
 * by Jorgen Lundman <lundman@lundman.net> 
 *
 * Opens a listening socket and awaits connection. When one is received,
 * it awaits input in the form of "ls -l /some/path/" and returns the
 * directory listing of such.
 * 
 * 11/02/2003 - epoch
 *
 */

#include <stdio.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>

#include "lion.h"
#include "misc.h"

#include "dirlist.h"

static int master_switch = 0;
static int server_port   = 57777;  // Set to 0 for any port.



int lion_userinput( lion_t *handle, 
					void *user_data, int status, int size, char *line)
{
	lion_t *new_handle = NULL;

	//	printf("userinput %p %d\n", handle, status);

	switch( status ) {
		
	case LION_CONNECTION_LOST:
		printf("Connection '%p' was lost.\n", handle);

		break;

		
	case LION_CONNECTION_CLOSED:
		printf("Connection '%p' was gracefully closed.\n", handle);


		break;
		
	case LION_CONNECTION_NEW:
		printf("Connection '%p' has a new connection...\n", handle);

		// Accept the port
		new_handle = lion_accept(handle, 0, 0, NULL, NULL, NULL);

		break;

	case LION_CONNECTION_CONNECTED:
		printf("Connection '%p' is connected. \n", handle);

		break;


	case LION_BUFFER_USED:
		// If user_data is set (ie, file->socket) we pause the file.
		//		if (user_data)
		//		lion_disable_read( user_data );
		break;

	case LION_BUFFER_EMPTY:
		// If user_data is set (ie, file->socket) we resume the file.
		if (user_data)
			lion_enable_read( user_data );
		break;

	case LION_INPUT:
		//		printf("[input] on %p -> '%s'\n", handle, line);


		if (!strncasecmp("ls ", line, 3)) {
			char *flags, *ar;

			ar = &line[3];
			// grab the flags wanted
			flags = misc_digtoken(&ar, " -\r\n");
			
			if (flags) {

				dirlist_list(handle, 
							 // Pass it handle so THIS userinput is called
							 ar, NULL,
							 dirlist_a2f(flags)|DIRLIST_USE_CRNL, 
							 handle);
#if 0
				dirlist_list(handle, 
							 // Pass it handle so THIS userinput is called
							 ar, 
							 DIRLIST_SHORT|DIRLIST_PIPE, 
							 handle);
				dirlist_list(handle, 
							 // Pass it handle so THIS userinput is called
							 ar, 
							 DIRLIST_SHORT|DIRLIST_PIPE, 
							 handle);
#endif
				break;
			}
			
			lion_printf(handle, "parse error: see HELP\r\n");
			
		}

		if (!strncasecmp("help", line, 4)) {
			lion_printf(handle, "LION contrib/libdirlist tester\r\n");
			lion_printf(handle, "usage: ls -flags /directory/path/\r\n");
			lion_printf(handle, "     flags are: \r\n"
						"    *-l long format\r\n"
						"     -1 short format\r\n"
						"     -X XML format\r\n"
						"     -T print to temporary file, return name\r\n"
						"    *-P print to pipe\r\n"
						"     -N sort by name\r\n"
						"     -t sort by date\r\n"
						"     -s sort by size\r\n"
						"     -C sort with case insensitive\r\n"
						"     -r sort in reverse\r\n"
						"     -R sort recursively\r\n"
						"     -D sort directories before files\r\n"
						"     -a display dot-entries\r\n\r\n");
		}

		if (!strncasecmp("quit", line, 4)) {

			lion_close(handle);
			break;

		}



		if (handle && user_data) { // reply from LS
			
			lion_printf(handle, "%s\n", line);

		}


		break;

	case LION_BINARY:
		break;
	}



	return 0;

}








void exit_interrupt(void)
{

	master_switch = 1;

}






int main(int argc, char **argv)
{
	void *listen = NULL;


	signal(SIGINT, exit_interrupt);
#ifndef WIN32
	signal(SIGHUP, exit_interrupt);
#endif



	printf("Initialising Network...\n");

	lion_init();
	lion_compress_level( 0 );

	printf("Network Initialised.\n");



	// Create an initial game
		
	printf("Initialising Socket...\n");


	listen = lion_listen(&server_port, 0, 0, NULL);


	if (!listen) {
		
		printf("Socket Failed...\n");
		master_switch = 1;
		
	}
	
	
	printf("Listening on port %d...\n", server_port);


	printf("Initialising libdirlist...\n");

	if (dirlist_init(1)) {

		printf("libdirlist failed...\n");
		master_switch = 1;

	}



	while( !master_switch ) {

		// If you are using rate calls, you should sleep for 1s or less.
		lion_poll(0, 1);     // This blocks. (by choice, FYI).
		//printf("main\n");
	}
	printf("\n");

	
	printf("Releasing libdirlist...\n");

	dirlist_free();


	printf("Releasing Socket...\n");
	if (listen) {
		lion_disconnect(listen);
		listen = NULL;
	}
	printf("Socket Released.\n");


	lion_free();

	printf("Network Released.\n");

	printf("Done\n");

	return 0; // Avoid warning

}
