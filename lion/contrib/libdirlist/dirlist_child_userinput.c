
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


#include "lion.h"
#include "misc.h"

#include "dirlist_child.h"
#include "dirlist_child_userinput.h"
#include "dirlist_child_process.h"


int dirlist_child_userinput( lion_t *handle, void *user_data, 
							 int status, int size, char *line)
{

	switch ( status ) {

	case LION_PIPE_EXIT:
#ifdef DEBUG
		printf("  [dirlist_child] parent is gone, clean exit.\n");
#endif
		child_switch = 1;
		break;


	case LION_PIPE_FAILED:
#ifdef DEBUG
		printf("  [dirlist_child] parent %p failed! %d:%s\n",
			   handle, size, line);
#endif
		child_switch = 1;
		break;

	case LION_PIPE_RUNNING:
#ifdef DEBUG
		printf("  [dirlist_child] parent %p is running\n", handle);
#endif
		break;


	case LION_INPUT:
#ifdef DEBUG
		printf("  [dirlist_child] parent %p -> '%s'\n", handle, line);
#endif
		dirlist_child_parse(handle, line);

		break;

	case LION_BUFFER_USED:
#ifdef DEBUG
		printf("  [dirlist_child] buffering event ignored\n");
#endif
		break;


	default:
		break;


	}
	
	return 0;

}



void dirlist_child_parse( lion_t *reply, char *task)
{
	char *token, *ar;
	int id, flags, precat;

	// task = "<id> <flags> <path>" 
	ar = task;

	if (!(token = misc_digtoken(&ar, " "))) {
		lion_printf(reply, "0 -0 Parse Error: no id field\n");
		return;
	}

	id = atoi( token );

	if (!(token = misc_digtoken(&ar, " "))) {
		lion_printf(reply, "%u -0 Parse Error: no flags field\n",
					id);
		return;
	}

	flags = atoi( token );

	if (!(token = misc_digtoken(&ar, " "))) {
		lion_printf(reply, "%u -0 Parse Error: no precat field\n",
					id);
		return;
	}

	precat = atoi( token );

	// file name
#if 0
	if (!(token = misc_digtoken(&ar, " \""))) {
		lion_printf(reply, "%u -0 Parse Error: no path field\n",
					id);
		return;
	}
#endif
	// We KNOW the "ar" should have a space now (we control the send of this
	// string from parent after all) the rest is unknown, and may have spaces.
	//	printf("aaa ar is '%s' and precat %d\n", ar, precat);

	if (precat) {

#ifdef DEBUG
		if (ar[precat] != ' ')
			printf("[dirlist] Not a space at precat?\n");
#endif

		ar[precat] = 0;
		precat++;
	}

	// We have id, flags and path.

	dirlist_child_process(reply, id, flags, 
						  precat ? ar : "",
						  &ar[precat]);

}



