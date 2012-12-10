
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
 * template example
 *
 * by Jorgen Lundman <lundman@lundman.net> 
 *
 * template that can be used for more serious application. Creates new
 * nodes for each connection etc. Listens on a socket, and just relays
 * traffic, like a simple chat room.
 * 
 * 24/01/2003 - epoch
 *
 */

#include <stdio.h>
#include <fcntl.h>
#include <signal.h>

#include "lion.h"

#include "object.h"



static int master_switch = 0;
static int server_port   = 57777;  // Set to 0 for any port.


// The master listening socket. This could be a node as well, but
// since we only have one we have it here. We will only get _NEW from
// this one socket.
static object_t *listen = NULL;





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

	listen = object_new();
	listen->type = OBJECT_TYPE_LISTEN;

	listen->handle = lion_listen(&server_port, NULL, LION_FLAG_NONE, 
								 (void *) listen);


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

		if (listen->handle) {
			lion_disconnect(listen->handle);
			listen->handle = NULL;
		}

		object_free(listen);
		listen = NULL;
	}
	printf("Socket Released.\n");


	object_free_all();

	lion_free();

	printf("Network Released.\n");

	printf("Done\n");

	return 0; // Avoid warning

}
