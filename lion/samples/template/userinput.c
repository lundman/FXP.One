
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

#include "userinput.h"



//
// This is the function called by lion library when there is an event.
// To demonstrate how to write code that calls different places depending on
// the type of the object node, we split the logic here.
//


int lion_userinput( lion_t *handle, 
					void *user_data, int status, int size, char *line)
{
	object_t *node = (object_t *) user_data;  // Get our object struct


	// Basically, if we have no user_data node, and there for no object node
	// we just exit this function. This should never happen, since we always
	// pass a valid node of our own. But just incase, instead of core dumping..

	if (!user_data)
		return 0;


	// Now, check the type of the object node, and call the appropriate
	// function.

	switch (node->type) {

	case OBJECT_TYPE_NONE:   // Unknown type, shouldn't happen, so do nothing.
		return 0;

	case OBJECT_TYPE_LISTEN: // Our master listening socket.
		return listen_userinput( handle, user_data, status, size, line);

	case OBJECT_TYPE_UNREGISTERED: // An "unregistered" connection.
		return unreg_userinput( handle, user_data, status, size, line);

	case OBJECT_TYPE_REGISTERED: // An "registered" connection.
		return reg_userinput( handle, user_data, status, size, line);

	}

	// NOT REACHED
	return 0; // Avoid warning.
}


