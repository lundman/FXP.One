
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
 * it opens a file (this source file) and sends it to the remote connection.
 * Closes the connection when the file is done.
 * 
 * 09/01/2003 - epoch
 * 22/01/2003 - converting to lib lion syntax
 *
 */

#include <stdio.h>
#include <fcntl.h>
#include <signal.h>

#include "lion.h"


static int master_switch = 0;
static int server_port   = 57777;  // Set to 0 for any port.


#ifdef WIN32
#define FILENAME "../send-file.c"
//#define FILENAME "Debug/sendfile.exe"
#else
#define FILENAME "send-file.c"
#endif



//#define RATE_OUT 4



int lion_userinput( lion_t *handle, 
				   void *user_data, int status, int size, char *line)
{
	lion_t *new_handle = NULL;
	lion_t *file = NULL;
	float out;

	switch( status ) {

	case LION_FILE_FAILED:
		printf("[send] file failed event!\n");
		if (user_data) {
			lion_printf(user_data, "%s\r\n", line);
			lion_close(user_data);
		}
		break;

	case LION_FILE_CLOSED:
		printf("[send] file close event!\n");
		// If a file finished, and we had a user_data (ie, socket) close it
		if (user_data) { 
			lion_close(user_data);
		}
		break;
	case LION_FILE_OPEN:
		printf("[send] file open event!\n");
		break;
		
	case LION_CONNECTION_LOST:
		printf("Connection '%p' was lost.\n", handle);
		break;
		
	case LION_CONNECTION_CLOSED:
		printf("Connection '%p' was gracefully closed.\n", handle);

		lion_get_cps(handle, NULL, &out);
		printf("Send took %lu seconds, at %7.2fKB/s\n",
			   lion_get_duration(handle), out);
		break;
		
	case LION_CONNECTION_NEW:
		printf("Connection '%p' has a new connection...\n", handle);

		// Accept the port, use LION_FLAGS_NONE instead of 0.
		new_handle = lion_accept(handle, 0, 0, NULL, NULL, NULL);

		break;

		// We have a new connection, send the greeting:
		// This isn't called here. We leave it for debugging reasons.
	case LION_CONNECTION_CONNECTED:
		printf("Connection '%p' is connected. \n", handle);

		// Open a new file, and send it. Set binary to do chunked mode.
		// (faster, but text will work too).
		// If we can't open file, close socket.
		// pass the "handle" as user_data. Then if we
		// get IO on the new handle, its user_data will point back to here.
		file = lion_open(FILENAME, O_RDONLY, 0600, 0, handle);
		
		// We can can check if file failed here, or use the _LOST event.
		// since _LOST event gives us the error string, it is better.
		if (!file) {  
			return 0;
		}

		lion_set_userdata(handle, file);

		lion_setbinary(file);
		lion_setbinary(handle); // Technically not needed.

#ifdef RATE_OUT
		printf("setting rate_out limit\n");
		lion_rate_out( handle, RATE_OUT );
#endif

		break;

	case LION_BUFFER_USED:
		// If user_data is set (ie, file->socket) we pause the file.
		//printf("buffer used, disable %p\n", user_data);
		printf("used\n");
		if (user_data)
			lion_disable_read( user_data );
		
		break;

	case LION_BUFFER_EMPTY:
		//printf("buffer empty, enable %p\n", user_data);
		// If user_data is set (ie, file->socket) we resume the file.
		printf("free\n");
		if (user_data)
			lion_enable_read( user_data );
		break;

	case LION_INPUT:
		// Not used, this sample uses binary.

		// If you want to send text mode, you can use
		//	net_printf(user_data, "%s\r\n", line);
		break; 
	case LION_BINARY:
		// We got input, is "user_data" set? then send it to them
		// That means file -> socket. But data from socket is not sent to file.
		//printf("input %p %p\n", handle, user_data);
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
	lion_t *listen = NULL;


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


	while( !master_switch ) {

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

	getchar();
	return 0; // Avoid warning

}
