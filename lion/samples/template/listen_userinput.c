
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

#include <stdio.h>

#include "lion.h"

#include "object.h"





//
// This is called from our userinput.c if the type of object is listen.
//
int listen_userinput( lion_t *handle, 
					void *user_data, int status, int size, char *line)
{
	object_t *node = (object_t *) user_data;  // Get our object struct
	object_t *newo = NULL;

	// We should _always_ have a object node here, since we are only called
	// from userinput.c if we did have one. But, for sanity....

	if (!node)
		return 0;




	// Check which event we got and process...

	switch( status ) {

		
	case LION_CONNECTION_LOST:
		printf("Listening socket '%p' was lost: %d:%s\n", handle, size, line);
		break;
		
	case LION_CONNECTION_CLOSED:
		printf("Listening socket '%p' was gracefully closed.\n", handle);
		break;
		
	case LION_CONNECTION_NEW:
		printf("Listening socket '%p' has a new connection...\n", handle);


		// Get a new object container...
		newo = object_new();

		// Set their type to unregistered. The connection messages goes there.
		newo->type = OBJECT_TYPE_UNREGISTERED;

		// Accept the port
		newo->handle = lion_accept(handle, 0, LION_FLAG_NONE, (void *)newo, 
								   NULL, NULL);

		// Lets allow SSL'ed connections too.
		lion_ssl_set(newo->handle, LION_SSL_SERVER);

		break;

	case LION_CONNECTION_CONNECTED:
		break;

	case LION_BUFFER_USED:
		break;

	case LION_BUFFER_EMPTY:
		break;

	case LION_INPUT:
		break; 

	case LION_BINARY:
		break;

	}



	return 0;

}


