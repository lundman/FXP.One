
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
#include <string.h>

#include "lion.h"

#include "object.h"
#include "command.h"

#include "say.h"




//
// This is called from our userinput.c if the type of object is unregistered
//
int reg_userinput( lion_t *handle, 
				   void *user_data, int status, int size, char *line)
{
	object_t *node = (object_t *) user_data;  // Get our object struct

	// We should _always_ have a object node here, since we are only called
	// from userinput.c if we did have one. But, for sanity....

	if (!node)
		return 0;




	// Check which event we got and process...

	switch( status ) {

		
	case LION_CONNECTION_LOST:
		printf("Connection '%p' (%s) was lost: %d:%s\n", handle, 
			   node->username,
			   size, line);

		say_all("%s connection lost: %d:%s\r\n", node->username, size, line);

		node->handle = NULL;
		object_free( node );

		break;
		
	case LION_CONNECTION_CLOSED:
		printf("Connection '%p' (%s) was gracefully closed.\n", handle,
			   node->username);

		say_all("%s disconnected.\r\n", node->username);

		node->handle = NULL;
		object_free( node );
		break;
		
		// We have a new connection, send the greeting:
		// This isn't called here. We leave it for debugging reasons.
	case LION_CONNECTION_CONNECTED:
		break;

	case LION_BUFFER_USED:
		break;

	case LION_BUFFER_EMPTY:
		break;

	case LION_INPUT:

		if (!strncasecmp("quit", line, 4)) {
			lion_close(handle);
			break;
		}

		if (!strncasecmp("who", line, 4)) {
			command_who(node);
			break;
		}

		say_all("<%s> %s\r\n", node->username, line);

		break; 

	case LION_BINARY:
		break;

	}



	return 0;

}


