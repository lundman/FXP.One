
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
 * send-file example
 *
 * by Jorgen Lundman <lundman@lundman.net> 
 *
 * Opens a listening socket and awaits connection. When one is received,
 * it opens a file in write mode, and any information received from remote
 * host is saved into the file. The file is closed when the connection is
 * dropped.
 * 
 * 09/01/2003 - epoch
 * 22/01/2003 - converting to lib lion syntax.
 *
 */

#include <stdio.h>
#include <fcntl.h>
#include <signal.h>

#include "lion.h"


static int master_switch = 0;
static int server_port   = 57777;  // Set to 0 for any port.

//#define CONNECT_METHOD 


#define RATE_LIMIT 20



int lion_userinput( lion_t *handle, 
					void *user_data, int status, int size, char *line)
{
	lion_t *new_handle = NULL;
	lion_t *file = NULL;

	switch( status ) {
		
	case LION_CONNECTION_LOST:
		printf("Connection '%p' was lost.\n", handle);

		// If a file finished, and we had a user_data (ie, socket) close it
		if (user_data)
			lion_close(user_data);
		break;

	case LION_FILE_FAILED:
		// If the file failed to open, report reason.
		printf("File failed\n");
		if (user_data) {
			lion_printf(user_data, "%s\r\n", line);
			lion_close(user_data);
		}
		break;
		
	case LION_CONNECTION_CLOSED:
		printf("Connection '%p' was gracefully closed.\n", handle);

		// If a file finished, and we had a user_data (ie, socket) close it
		if (user_data)
			lion_close(user_data);
		
#ifdef RATE_LIMIT
		{
			float in;
			lion_get_cps(handle, &in, NULL);
			printf("Send took %lu seconds, at %7.2fKB/s\n",
				   lion_get_duration(handle), in);
		}
#endif
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

		// Socket is connected, disable its read so that we don't lose
		// data while we wait for the file to open.
		
		lion_disable_read( handle );

#ifdef RATE_LIMIT
		lion_rate_in(handle, RATE_LIMIT);
#endif

		
		// Open a file to write to.
		
		file = lion_open("test-input.tmp", O_WRONLY|O_CREAT|O_TRUNC, 
						 0600, LION_FLAG_FULFILL, (void *)handle);
		

		// If we use LION_FLAG_FULFILL above, we do _not need_ to check for
		// NULL node, since it will always return a valid node, even if
		// we get _FAILED event next loop.
		// If you dont specify that flag, you will get NULL for immediate
		// errors, and you will NOT receive an event. You will receive
		// _OPEN event however.

		if (!file) {  
			lion_printf(handle, "Failed to open file\r\n");
			lion_close(handle);
			return 0;
		}
		
		// Change them into binary, for efficiency
		lion_setbinary(file);
		lion_setbinary(handle); 
		
		// Assign the new file to socket's user_data so we can relay
			// data later.
		lion_set_userdata(handle, (void *)file);
		
		// Since we don't want to read from this file, and don't want
		// lnet to close it because it thinks it is empty, disable_read
		lion_disable_read( file );
		break;

	case LION_FILE_OPEN:
		if (user_data)
			lion_enable_read( user_data );
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
		// Using binary in this example
		break;

	case LION_BINARY:
		// We got input, is "user_data" set? then send it to them
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


#ifdef CONNECT_METHOD
	listen = lion_connect("lundman.dyndns.org", 57777, 0, 0, 0, NULL);
#else
	listen = lion_listen(&server_port, 0, 0, NULL);
#endif

	if (!listen) {
		
		printf("Socket Failed...\n");
		master_switch = 1;
		
	}
	
	
	printf("Listening on port %d...\n", server_port);



	while( !master_switch ) {

		// If you are using rate calls, you should sleep for 1s or less.
		lion_poll(0, 1);     // This blocks. (by choice, FYI).

	}
	printf("\n");


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
