
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
 * udp-pong example
 *
 * by Jorgen Lundman <lundman@lundman.net> 
 *
 * Opens a new UDP socket, awaits input, sends output.
 * 
 * 22/04/2003 - epoch
 *
 */

#include <stdio.h>
#include <fcntl.h>
#include <signal.h>

#include "lion.h"


static int master_switch = 0;

static int server_port   = 57777;  // Set to 0 for any port.
lion_t *listenx = NULL;

lion_t *second = NULL;







int lion_userinput( connection_t *handle, 
					void *user_data, int status, int size, char *line)
{

	printf("handler called: %p -> %p: %d\n", handle, user_data, status);

	switch( status ) {
		
	case LION_CONNECTION_LOST:
		printf("Connection '%p' was lost. %d:%s\n", handle,
			   size, line);

		if (handle == listenx)
			listenx = NULL;
		if (handle == second)
			second = NULL;

		break;
		
	case LION_CONNECTION_CLOSED:
		printf("Connection '%p' was gracefully closed.\n", handle);

		if (handle == listenx)
			listenx = NULL;
		if (handle == second)
			second = NULL;

		break;
		

		// It was successfully opened.
	case LION_CONNECTION_CONNECTED:
		printf("Connection '%p' is connected. \n", handle);

		break;

	case LION_BUFFER_USED:
		break;

	case LION_BUFFER_EMPTY:
		break;

	case LION_INPUT:

		{
			unsigned long host;
			int port;

			lion_getpeername(handle, &host, &port);
			
			printf("<%p> input '%s' from %08lx:%d\n", handle, line,
				   host, port);

			if (!strcmp("help", line)) {
				lion_printf(handle, "Help yourself!\n");


				if (!second) {
					printf("Assigning second\n");

					second = lion_udp_bind_handle(handle, handle, (void *)0x12345678);

					printf("Bound second to %p\n", second);

				}

			}

			if (!strcmp("quit", line)) {
				lion_close(handle);
			}

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



	listenx = lion_udp_new(&server_port, 0, 0, NULL);


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

	if (second) {
		lion_disconnect(second);
		second = NULL;
	}

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
