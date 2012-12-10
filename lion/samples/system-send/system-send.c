
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
 * system example
 *
 * by Jorgen Lundman <lundman@lundman.net> 
 *
 * Spawn a new process, send it some info, receive data.
 * 
 * 10/01/2003 - epoch
 * 22/01/2003 - converted to use lion syntax
 *
 */

#include <stdio.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>

#include "lion.h"


static int master_switch = 0;

static lion_t *remote = NULL;
static lion_t *system = NULL;



int lion_userinput( lion_t *handle, 
					void *user_data, int status, int size, char *line)
{

	// Check if this input is for parent, or for child.
	// if this event comes from "child" we must be the parent.
	switch( status ) {
		
	case LION_PIPE_FAILED:
		printf("child failed to start - %d:%s\n", size, line);
		system = NULL;
		master_switch = 1;
		break;

	case LION_PIPE_RUNNING:
		printf("child is running\n");
		break;

	case LION_PIPE_EXIT:
		printf("child finished\n");
		// Close the other one as well.
		system = NULL;

		if (remote) {
			lion_close(remote);
			remote=NULL;
		}
		break;


	case LION_CONNECTION_LOST:
		printf("Failed to connect! - %d:%s\n", size, line);
		master_switch = 1;
		break;
		
	case LION_CONNECTION_CLOSED:
		// If we really want return code, we should call lion_want_status().
		printf("Remote connection closed.\n");

		remote = NULL;

		if (system) {
			lion_close(system);
			system=NULL;
		}

		// Since we've sent the file and closed our only connection, lets exit
		master_switch = 1;

		break;
		
	case LION_CONNECTION_NEW:
		break;
		
	case LION_CONNECTION_CONNECTED:
		
		printf("Connection to remote host successful!\n");
		
		// Call system to send us some large-ish file
		system = lion_system("/bin/cat /bin/sh", 0, 0, remote);
		lion_set_userdata(remote, system);
		lion_setbinary(remote);
		lion_setbinary(system);
		
		break;
		
	case LION_BUFFER_USED:
		if (user_data)
			lion_disable_read( user_data );
		break;
		
	case LION_BUFFER_EMPTY:
		if (user_data)
			lion_enable_read( user_data );
		break;
		
	case LION_BINARY:
		if (user_data)
			lion_output(user_data, line, size); 
		break;
		
	case LION_INPUT:
		printf("[system]: '%s'\n", line);

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
		
	printf("Connecting to localhost:57777...\n");


	remote = lion_connect("127.0.0.1", 57777, NULL, 0, 0, NULL);

	
	// Two out comes.
	// Parent.
	//         child has child node, parent is NULL.
	// Child.
	//         child is NULL, and parent is the node to parent.


	while( !master_switch ) {

		lion_poll(0, 1);     // This blocks. (by choice, FYI).

	}
	printf("\n");

	// If we are parent, release child
	if (remote)
		lion_disconnect(remote);

	lion_free();

	printf("Network Released.\n");

	printf("Done\n");

	// For fun, lets return a random return code 
	return 0;

}
