
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

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#include "lion.h"
#include "misc.h"

#include "dirlist.h"
#include "dirlist_userinput.h"



int dirlist_userinput( lion_t *handle, void *user_data, 
					   int status, int size, char *line)
{
	dirlist_child_t *child;

	child = (dirlist_child_t *) user_data;

	switch ( status ) {

	case LION_PIPE_EXIT:
		child->handle = NULL;
		break;

	case LION_PIPE_FAILED:
#ifdef DEBUG
		printf("[dirlist] child %p failed! %d:%s\n",
			   handle, size, line);
#endif
		if (size == -1)
			lion_want_returncode( handle );

		child->handle = NULL;
		break;

	case LION_PIPE_RUNNING:
#ifdef DEBUG
		printf("[dirlist] child %p is running\n", child);
#endif
		break;


	case LION_INPUT:
		//printf("[dirlist] child %p '%s'\n", child, line);
		
		dirlist_parse(handle, child, line);

		break;

	default:
		;
	}
	
	return 0;

}


void dirlist_parse( lion_t *parse, dirlist_child_t *child, char *line)
{
	char *ar, *token;
	int id, returncode = 0;
	dirlist_queue_t *que;
	char buffer[1024];

	// If queue_head has stop_msg set, check if they are equal,
	// if set but not equal, relay traffic up to application.
	// if both are false, attempt to parse.
	if (child->queue_head && child->queue_head->stop_msg) {

		if (!strcmp(child->queue_head->stop_msg, line)) {

#ifdef DEBUG
			printf("Got stop message\n");
#endif

			que = dirlist_queue_pop( &child->queue_head, &child->queue_tail );
			
			if (que->user_handle) {

				lion_get_handler( que->user_handle )(
													 que->user_handle,
													 que->user_data,
													 LION_INPUT,
													 4, 
													 ":END");
			}

			dirlist_queue_free( que );
			child->requests--;

			return;
		}

		// Relay traffic.
		if (child->queue_head->user_handle) {

			lion_get_handler( child->queue_head->user_handle )(
						  child->queue_head->user_handle,
						  child->queue_head->user_data,
						  LION_INPUT,
						  strlen(line), 
						  line);
		}

		return;

	} // stop_msg


	ar = line;
	
	if (!(token = misc_digtoken( &ar, " " ))) {
		return ;
	}

	id = atoi( token );

	// This should not happen.
	if (!child || !child->queue_head)
		return;

	if (id != child->queue_head->id) {
#ifdef DEBUG
		printf("[dirlist_parse] id don't match head of list's id\n");
#endif
		return;
	}

	// Check if the next word is "start"
	if ((token = misc_digtoken( &ar, " " ))) {
		if (!mystrccmp("start", token)) {

			// We need to start to relay data, until we get the
			// "<id> stop" message.
			char stop_msg[40]; // longest int 21 plus " stop".

			sprintf(stop_msg, "%u stop", id);
			child->queue_head->stop_msg = strdup(stop_msg);
			
			if (child->queue_head->user_handle) {

				lion_get_handler( child->queue_head->user_handle )(
							  child->queue_head->user_handle,
							  child->queue_head->user_data,
							  LION_INPUT,
							  16, 
							  ":0 -list follows");
			}

			return;
		}


		returncode = atoi(token);


	} // digtoken
	







	que = dirlist_queue_pop( &child->queue_head, &child->queue_tail );


#ifdef DEBUG
	printf("[dirlist_parse] delivering result to client\n");

	printf("  %u %s\n", id, ar);
#endif

	// should reply:
	// ":0 /tmp/filename.txt"      0 is success.
	// ":errno The string error"   for failure, like:
	// ":13 Permission denied".


	// We need to reply with ":%d %s", returncode, ar
	// the string is strlen(ar) + SIZE_INT(returncode) + 1 + 1 + 1 
	// ( colon + space + null )
#ifndef WIN32
	snprintf( buffer, sizeof(buffer), ":%d %s", returncode, ar );
#else
	sprintf( buffer,":%d %s", returncode, ar );
#endif
	
	if (que->user_handle) {
		
		lion_get_handler( que->user_handle )(
											 que->user_handle,
											 que->user_data,
											 LION_INPUT,
											 strlen(buffer), 
											 buffer);
	}
	
	dirlist_queue_free( que );
	child->requests--;
}
