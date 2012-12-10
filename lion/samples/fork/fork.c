
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
 * fork example
 *
 * by Jorgen Lundman <lundman@lundman.net> 
 *
 * fork's a child process, and sends it a string to which the child replies.
 * 
 * 09/01/2003 - epoch
 * 22/01/2003 - converting into lion syntax
 *
 */

#include <stdio.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>

#include "lion.h"


static int master_switch = 0;
static int child_switch = 0;

static lion_t *parent = NULL; // The node to talk TO the parent, used by child.
static lion_t *child = NULL;  // The node to talk TO the child, used by parent.




void exit_interrupt(void)
{

	master_switch = 1;

}







int lion_userinput( lion_t *handle, 
					void *user_data, int status, int size, char *line)
{
	
	printf("[event] get %p and %d for %p  (%p,%p)\n", handle, status, user_data, parent, child);

		
		switch( status ) {
			
		case LION_PIPE_FAILED:
			printf("[parent] child lost: %s\n", line);
			child = NULL;
			break;
			
		case LION_PIPE_EXIT:
			printf("[parent] child has exited\n");

			if (size == -1) { 
				// We didn't get the returncode, lets ask for it and
				// we will, eventually, get a second _CLOSED event with it.
				printf("[parent] no return code, requesting it.\n");
				lion_want_returncode(handle);
				break;

			}

			// We know the return code (finally)
			printf("[parent] child exit - returncode %d\n", size);

			child = NULL;
			break;
			
		case LION_PIPE_RUNNING:
			printf("[parent] child is running, apparently\n");
			lion_printf(handle, "Hello there my child!\n");
			break;
			
		case LION_BUFFER_USED:
			break;
			
		case LION_BUFFER_EMPTY:
			break;
			
		case LION_BINARY:
			// Using text mode in this example
			break;

		case LION_INPUT:

			printf("[parent] child says '%s'\n", line);

			if (!strncmp("Hello ", line, 6)) {  // reply

				lion_printf(handle, "quit\n");

			}
			break;
		}





	return 0;

}



int lion_userinput_child( lion_t *handle, 
					void *user_data, int status, int size, char *line)
{
	
	printf("[event] get %p and %d for %p  (%p,%p)\n", handle, status, user_data, parent, child);




		switch( status ) {
			
		case LION_PIPE_FAILED:
			printf("[child] parent lost: %s\n", line);
			child_switch = 1;
			parent = NULL;
			break;
			
		case LION_PIPE_EXIT:
			printf("[child] parent has exited\n");
			child_switch = 1;
			parent = NULL;
			break;
			
		case LION_PIPE_RUNNING:
			printf("[child] parent is running.\n");
			break;
			
		case LION_BUFFER_USED:
			break;
			
		case LION_BUFFER_EMPTY:
			break;
			
		case LION_BINARY:
			// Using text mode in this example
			break;

		case LION_INPUT:

			printf("[child] parent says '%s'\n", line);

			if (!strncmp("Hello ", line, 6)) {  // reply

				lion_printf(handle, "Hello there my parent, I hate you!\n");

			} else if (!strncmp("quit", line, 4)) {  // reply

				lion_printf(handle, "bye!\n");
				child_switch = 1;

			}

			break;
		}

	return 0;

}






int main_child(lion_t *node, void *user_data, void *arg)
{
	printf("[child] looping\n");
	

	parent = node;
	lion_set_handler(parent, lion_userinput_child);



	while( !child_switch ) {

		lion_poll(0, 5);     // This blocks. (by choice, FYI).

	}
	printf("\n");

	// If we are parent, release child
	if (parent) {
		lion_disconnect(parent);
		parent = NULL;
	}

	//lion_exitchild(rand());
	return rand();
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

	printf("Parent starting...\n");

	
	// Fork here, should have parent and child running.
	// child is set to child handle in parent, and NULL in child.
	// and &parent gets set to parent handle for the child.
	child = lion_fork( main_child, LION_FLAG_NONE, NULL, NULL);

	//parent = NULL;

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
	if (child)
		lion_disconnect(child);

	lion_free();

	printf("Network Released.\n");

	printf("Done\n");

	getchar();
	// For fun, lets return a random return code 
	return 0;
}
