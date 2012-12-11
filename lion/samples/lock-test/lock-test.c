
/*- 
 * 
 * New BSD License 2006
 *
 * Copyright (c) 2012, Jorgen Lundman
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
 * lock-test example
 *
 * by Jorgen Lundman <lundman@lundman.net> 
 *
 * Open the same file twice, using LION_EXCLUSIVE, the second open should
 * fail, if the locking OS code in file.c is correct.
 * 
 * 11/12/2012 - epoch
 *
 */

#include <stdio.h>
#include <fcntl.h>
#include <signal.h>

#include "lion.h"


static int master_switch = 0;



int lion_userinput( lion_t *handle, 
					void *user_data, int status, int size, char *line)
{
	lion_t *new_handle = NULL;
	lion_t *file = NULL;

	switch( status ) {
		
	case LION_CONNECTION_LOST:
		printf("Connection '%p' was lost.\n", handle);
		break;

	case LION_FILE_FAILED:
		// If the file failed to open, report reason.
		printf("File failed\n");
		break;
		
	case LION_CONNECTION_CLOSED:
		printf("Connection '%p' was gracefully closed.\n", handle);
		break;
		
	case LION_CONNECTION_NEW:
		printf("Connection '%p' has a new connection...\n", handle);
		break;

		// We have a new connection, send the greeting:
		// This isn't called here. We leave it for debugging reasons.
	case LION_CONNECTION_CONNECTED:
		printf("Connection '%p' is connected. \n", handle);
		break;

	case LION_FILE_OPEN:
		printf("File open\n");
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
	lion_t *file1;
	lion_t *file2;

	signal(SIGINT, exit_interrupt);
#ifndef WIN32
	signal(SIGHUP, exit_interrupt);
#endif



	printf("Initialising Network...\n");

	lion_init();
	lion_compress_level( 0 );

	printf("Network Initialised.\n");



	// Create an initial game
		
	printf("Opening file 'test.bin' with exclusive...\n");

	file1 = lion_open("test.bin", 
			  O_WRONLY|O_CREAT,
			  (mode_t) 0644,
			  LION_FLAG_EXCLUSIVE,
			  NULL);
	if (!file1) {
	  printf("failed to open file1. Can I write to CWD?\n");
	  exit(0);
	}

	printf("file1 opened correctly!\n");

	printf("Re-opening file 'test.bin' with exclusive...\n");

	file2 = lion_open("test.bin", 
			  O_WRONLY|O_CREAT,
			  (mode_t) 0644,
			  LION_FLAG_EXCLUSIVE,
			  NULL);
	if (!file2) {
	  printf("fail2 failed to open, this is correct, and exclusive works properly on this platform\n");
	  exit(0);
	}

	printf("file2 opened. This is INCORRECT. File exclusive locking is broken on this platform\n");

	lion_free();

	printf("Done\n");

	return 0; // Avoid warning

}
