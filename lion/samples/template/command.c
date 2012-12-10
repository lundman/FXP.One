
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
#include "command.h"






int command_who_sub(object_t *node, void *arg1, void*arg2)
{
	static object_t *user;

	if (!arg1)
		return 0; // 0 to stop iterating.

	user = (object_t *) arg1;


	switch(node->type) {

	case OBJECT_TYPE_NONE:
		lion_printf(user->handle, "[unknown-type]\r\n");
		break;

	case OBJECT_TYPE_LISTEN:
		lion_printf(user->handle, "[ listening  ]\r\n");
		break;
	case OBJECT_TYPE_UNREGISTERED:
		lion_printf(user->handle, "[unregistered]\r\n");
		break;
	case OBJECT_TYPE_REGISTERED:
		lion_printf(user->handle, "[ registered ] %s\r\n", node->username);
		break;
	}


	return 1; // 1 to keep iterating

}


//
// Iterate the list of objects, and send them to the user.
//
void command_who(object_t *node)
{

	object_find(command_who_sub, (void *)node, NULL);

}
