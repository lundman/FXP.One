
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
 * SSL connect sample
 *
 * by Jorgen Lundman <lundman@lundman.net> 
 *
 * Connects to an SSL ip/port and tries to authenticate.
 * 
 * 28/01/2003 - epoch
 *
 */

#include <stdio.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>

#include "lion.h"


static int master_switch = 0;
static int server_port = 57777;

// Dont ever call a variable "listen" as it would over shadow listen().
static lion_t *listenX = NULL;



// Change the next define depending on which type of SSL you are testing
// this refers to logic flow, and in which order you receive the events.
//#define SSL_CONNECT_FIRST




int lion_userinput( lion_t *handle, 
					void *user_data, int status, int size, char *line)
{
	lion_t *new_handle;
		
	switch( status ) {
		
	case LION_CONNECTION_LOST:
		printf("[remote] connection lost: %d:%s\n", size, line);
		break;

	case LION_CONNECTION_CLOSED:
		printf("[remote] connection closed\n");
		break;
		
	case LION_CONNECTION_NEW:
		printf("[remote] NEW connection connected\n");
		new_handle = lion_accept(handle, 0, 0, NULL, NULL, NULL);
#ifndef SSL_CONNECT_FIRST
		lion_ssl_set(new_handle, LION_SSL_SERVER);
#endif
		break;

	case LION_CONNECTION_CONNECTED:
		printf("[remote] connection connected\n");
#ifdef SSL_CONNECT_FIRST
		lion_ssl_set(handle, LION_SSL_SERVER);
#endif
		break;

	case LION_CONNECTION_SECURE_ENABLED:
		printf("[remote] SSL successfully established, cipher: %s\n", line);
		break;

	case LION_CONNECTION_SECURE_FAILED:
		printf("[remote] SSL failed\n");
		break;

	case LION_BUFFER_USED:
		break;
		
	case LION_BUFFER_EMPTY:
		break;
		
	case LION_BINARY:
		// Using text mode in this example
		break;
		
	case LION_INPUT:
		
		printf("[remote] >> '%s'\n", line);

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

	lion_ssl_rsafile("example.pem");

	lion_init();
	lion_compress_level( 0 );

	printf("Network Initialised.\n");


	// Create an initial game
		
	printf("Initialising Socket...\n");

	
	listenX = lion_listen(&server_port, 0, 0, NULL);


	if (!listenX) {
		
		printf("Socket Failed...\n");
		master_switch = 1;
		
	}


	while( !master_switch ) {

		lion_poll(0, 1);     // This blocks. (by choice, FYI).

	}
	printf("\n");

	// If we are parent, release child
	if (listenX)
		lion_disconnect(listenX);

	lion_free();

	printf("Network Released.\n");

	printf("Done\n");

	// For fun, lets return a random return code 
	return 0;

}
