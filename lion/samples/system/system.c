
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
 * 22/01/2003 - converting to lion format.
 *
 */

#include <stdio.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>

#include "lion.h"


static int master_switch = 0;

static lion_t *child = NULL;


#define START_MANY 50


#ifdef WIN32
#define strncasecmp strnicmp
#define PROGRAM "cmd.exe"
#define COMMAND "DIR\nexit\n"
#else
#define PROGRAM "/bin/sh"
#define COMMAND "echo $$ ; who ; echo DONE ; exit 0\n"
#endif


int lion_userinput( lion_t *handle, 
					void *user_data, int status, int size, char *line)
{

	// Check if this input is for parent, or for child.
	// if this event comes from "child" we must be the parent.
	switch( status ) {
		
	case LION_PIPE_FAILED:
		printf("system failed to start! error %d '%s'\n", size, 
			   line ? line : "");
		break;
		
	case LION_PIPE_EXIT:
		// If we really want return code, we should call net_want_status().
		printf("system process has exited. %p - %d\n", handle,size);
		if (size == -1) lion_want_returncode(handle);
		break;
		
	case LION_PIPE_RUNNING:
		printf("system process is running...\n");
		lion_printf(handle, COMMAND);
		break;
		
	case LION_BUFFER_USED:
		break;
		
	case LION_BUFFER_EMPTY:
		break;
		
	case LION_BINARY:
		break;
		
	case LION_INPUT:
		printf("[system]: '%s'\n", line);


		// We can also just close the child, instead of asking it to exit
		if (!strncasecmp("DONE", line, 4)) {
			//			lion_close(child);
			child = NULL;
		}
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
	int i = 0;
	char *sargv[] = {
		PROGRAM,
		NULL
	};


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

	child = lion_execve(sargv[0], sargv, NULL, 1, 0, NULL);
	// This sample should actually use lion_system().
	
	// Two out comes.
	// Parent.
	//         child has child node, parent is NULL.
	// Child.
	//         child is NULL, and parent is the node to parent.


	while( !master_switch ) {

		lion_poll(0, 1);     // This blocks. (by choice, FYI).

#ifdef START_MANY
		if ((i++) == 10) {

			for (i = 0; i < START_MANY; i++) {

				child = lion_execve(sargv[0], sargv, NULL, 1, 0, NULL);
				
			}

			i = 20;

		}
#endif

	}
	printf("\n");

	// If we are parent, release child
	if (child)
		lion_disconnect(child);

	lion_free();

	printf("Network Released.\n");

	printf("Done\n");

	getchar();

	return 0;

}
