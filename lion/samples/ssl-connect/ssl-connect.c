
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
#include <sys/time.h>
#include "lion.h"

#define WITH_STDIN

static int master_switch = 0;

static lion_t *remote = NULL;

#ifdef WITH_STDIN
static lion_t *lstdin = NULL;
#endif

// Change the next define depending on which type of SSL you are testing
// this refers to logic flow, and in which order you receive the events.
// SSL_CONNECT_FIRST - Connect, wait for success, then issue SSL
// SSL_CONNECT_LATER - Connect and issue SSL
//#define SSL_CONNECT_FIRST
//#define SSL_CONNECT_LATER
//#define SSL_CONNECT_FTPD     // Talk a little FTP protocol
#define SSL_CONNECT_STARTTLS   // Talk STARTTLS (sendmail, pop)

int lion_userinput( lion_t *handle, 
					void *user_data, int status, int size, char *line)
{
	
		
	switch( status ) {
		
	case LION_CONNECTION_LOST:
		printf("[remote] connection lost: %d:%s\n", size, line);
		remote = NULL;
		break;

	case LION_CONNECTION_CLOSED:
		printf("[remote] connection closed\n");
		remote = NULL;
		master_switch = 1;
		break;
		
	case LION_CONNECTION_CONNECTED:
		printf("[remote] connection connected\n");
#ifdef SSL_CONNECT_FIRST
		lion_ssl_set(handle, LION_SSL_CLIENT);
#endif
		lion_printf(handle, "SSL\r\n");
		break;

	case LION_CONNECTION_SECURE_ENABLED:
		printf("[remote] SSL successfully established\n");
		break;

	case LION_CONNECTION_SECURE_FAILED:
		printf("[remote] SSL failed\n");
		// If plain is not acceptable, call lion_disconnect() here
		break;

	case LION_BUFFER_USED:
		break;
		
	case LION_BUFFER_EMPTY:
		break;
		
	case LION_BINARY:
		// Using text mode in this example
		break;
		
	case LION_INPUT:
		if (handle == remote) 
			printf("%s\n", line);
#ifdef WITH_STDIN
		else {
			lion_printf(remote,"%s\r\n", line);
		}
#endif
		fflush(stdout);

#ifdef SSL_CONNECT_FTPD
		if ((size > 4) && !strncmp("234 ", line, 4)) {
			printf("STARTING SSL\n");
			lion_ssl_set(handle, LION_SSL_CLIENT);
		}
#endif

#ifdef SSL_CONNECT_STARTTLS

		if ((size > 4) && !strncmp("220 2.0.0", line, 9)) {
			printf("Starting tls...\n");
			lion_ssl_set(handle, LION_SSL_CLIENT);
			return;
		}


		if ((size > 4) && !strncmp("220 ", line, 4)) {
			lion_printf(remote, "STARTTLS\r\n");
			printf("STARTTLS\n");
		}
#endif
		if ((size > 3) && !strncmp("SSL", line, 3)) {
			lion_ssl_set(handle, LION_SSL_CLIENT);
			printf("STARTTLS\n");
		}


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

	printf("Parent starting...\n");


	if (argc != 3) {
		
		remote = lion_connect("127.0.0.1", 56688, NULL, 0,  
							  LION_FLAG_FULFILL, NULL);

	} else {

		remote = lion_connect(argv[1], atoi(argv[2]), NULL, 0,  
							  LION_FLAG_FULFILL, NULL);

	}


#ifdef SSL_CONNECT_LATER
	lion_ssl_set(remote, LION_SSL_CLIENT);
#endif

#ifdef WITH_STDIN
	lstdin = lion_adopt(fileno(stdin), LION_TYPE_FILE, NULL);
#endif


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

	return 0;

}
