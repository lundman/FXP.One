
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
 * relaye example
 *
 * by Jorgen Lundman <lundman@lundman.net> 
 *
 * Opens a listening socket and awaits connection. When one is received,
 * it connects  to remote host:port and relays all data between the two.
 * Simple port  bounce
 * 
 * 18/01/2003 - epoch
 * 21/01/2003 - changed it to conform to lion syntax
 *
 */

#include <stdio.h>
#include <fcntl.h>
#include <signal.h>

#include "lion.h"


static int master_switch = 0;

static int server_port   = 57777;  // Set to 0 for any port.
lion_t *listenx = NULL;

char *remote_host = "whiterose.net";
char remote_port = 21;


int lion_userinput( connection_t *handle, 
					void *user_data, int status, int size, char *line)
{
	lion_t *new_handle = NULL;

	switch( status ) {
		
	case LION_CONNECTION_LOST:
		printf("Connection '%p' was lost.\n", handle);

		// A connection was lost, if we have "user_data" send the
		// reason across
		if (user_data) {
			lion_printf(user_data, "%s\r\n", line);
			lion_set_userdata(user_data, NULL);
			lion_close(user_data);
		}
		break;
		
	case LION_CONNECTION_CLOSED:
		printf("Connection '%p' was gracefully closed.\n", handle);

		// If a file finished, and we had a user_data (ie, socket) close it
		if (user_data) {
			lion_set_userdata(user_data, NULL);
			lion_close(user_data);
		}
		break;
		
	case LION_CONNECTION_NEW:
		printf("Connection '%p' has a new connection...\n", handle);

		// Accept the port
		new_handle = lion_accept(handle, 0, 0, NULL, NULL, NULL);

		break;

		// We have a new connection, send the greeting:
		// This isn't called here. We leave it for debugging reasons.
	case LION_CONNECTION_CONNECTED:
		printf("Connection '%p' is connected. \n", handle);

		// We get this msg for files too, so check it's actually a socket.
#if 1
		if (!user_data) {

			printf("Connecting...\n");
			new_handle = lion_connect(remote_host, remote_port, 
									  NULL, 0, 0, handle);
			lion_set_userdata(handle, new_handle);
			
			// Disable reading from this socket, until we are connected.
			lion_disable_read( handle );
			
			lion_setbinary(new_handle);
			lion_setbinary(handle); // Technically not needed.

		} else {
			
			lion_enable_read(user_data);

		}
#endif

		break;

	case LION_BUFFER_USED:
		// If user_data is set (ie, file->socket) we pause the file.
		if (user_data)
			lion_disable_read( user_data );
		break;

	case LION_BUFFER_EMPTY:
		// If user_data is set (ie, file->socket) we resume the file.
		if (user_data)
			lion_enable_read( user_data );
		break;

	case LION_INPUT:
		// Not used, this sample uses binary.
		printf("<%p> input '%s'\n", handle, line);
		// If you want to send text mode, you can use
		//	lion_printf(user_data, "%s\r\n", line);
		break; 
	case LION_BINARY:
		// We got input, is "user_data" set? then send it to them
		// That means file -> socket. But data from socket is not sent to file.

		if (user_data) 
			lion_output(user_data, line, size);

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



	listenx = lion_listen(&server_port, 0, 0, NULL);


	if (!listenx) {
		
		printf("Socket Failed...\n");
		master_switch = 1;
		
	}
	
	
	printf("Listening on port %d...\n", server_port);



	while( !master_switch ) {

		lion_poll(0, 1);     // This blocks. (by choice, FYI).

	}
	printf("\n");


	printf("Releasing Socket...\n");
	if (listenx) {
		lion_disconnect(listenx);
		listenx = NULL;
	}
	printf("Socket Released.\n");


	lion_free();

	printf("Network Released.\n");

	printf("Done\n");

	return 0; // Avoid warning

}
