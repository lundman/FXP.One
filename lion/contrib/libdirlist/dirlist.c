
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

#include "dirlist_child.h"

#include "queue.h"




__RCSID("$liddirlist: dirlist.c,v 1.28 2003/04/23 08:32:03 lundman Exp $");



dirlist_child_t *dirlist_child_head = NULL;



int dirlist_init(int num_helpers)
{
	int helper;
	dirlist_child_t *newd;
	lion_t *parent;

	//	num_helpers = 1;



	for( helper = 0; helper < num_helpers; helper++) {


		newd = (dirlist_child_t *) malloc(sizeof(*newd));

		if (!newd) return -1;

		memset(newd, 0, sizeof(*newd));
		newd->unique = 1; // Dont start at 0.

		parent = lion_fork( dirlist_child, LION_FLAG_FULFILL, (void *) newd,
							NULL);


		// Set our handler.
		lion_set_handler( parent, (lion_handler_t)dirlist_userinput );

		newd->handle = parent;

		newd->next = dirlist_child_head;
		dirlist_child_head = newd;


	}


	return 0;

}




void dirlist_free(void)
{
	dirlist_child_t *node, *next;
	dirlist_queue_t *que;


	for ( next = NULL, node = dirlist_child_head; node; node = next ) {

		// Release child - when child gets close, it exits.
		if (node->handle)
			lion_disconnect( node->handle );

		// For every que entry, release...
		while ((que = dirlist_queue_pop( &node->queue_head, &node->queue_tail ))) {

			dirlist_queue_free( que );

		}

		next = node->next;
		SAFE_FREE( node );

	}

	dirlist_child_head = NULL;

	dirlist_child_free();


}




int dirlist_list( lion_t *handle, char *path, char *precat,
				  int flags, void *user_arg )
{
	dirlist_child_t *node;
	dirlist_child_t *use_me = NULL;
	int lowest = 0;
	dirlist_queue_t *que;

	// Find a child to use...

	for ( node = dirlist_child_head; node; node = node->next ) {


		// handle should be !NULL
		// requests == 0
		// or, lowest request
		if (!node->handle) continue;

		// free, just use it.
		if ( !node->requests ) {  // Use this one

			use_me = node;
			break;

		}

		// Otherwise, look for the one with the lowest
		if (!lowest || (node->requests < lowest)) {
			lowest = node->requests;
			use_me = node;
		}

	}


	if (use_me) {

		node = use_me;

#ifdef DEBUG
		printf("[dirlist] picking child %p with %d requests\n", node,
			   node->requests);
#endif

		que = dirlist_queue_new( &node->queue_head, &node->queue_tail );

		if (!que)
			return -2;

		node->requests++;

		que->id = node->unique++;

		que->path = strdup( path );
		if (precat)
			que->precat = strdup(precat);
		que->flags = flags;
		que->user_data = user_arg;
		que->user_handle = handle;

#ifdef DEBUG
		printf("call from %p: flags %d\n", handle, flags);
#endif

		dirlist_send( node, que );

		return 0;
	}

	return -1; // none found

}



void dirlist_send(dirlist_child_t *node, dirlist_queue_t *que)
{

	lion_printf(node->handle,
				"%u %u %u %s %s\n",
				que->id, que->flags,
				que->precat ? strlen(que->precat) : 0,
				que->precat ? que->precat : "",
				que->path);

}




int dirlist_a2f(char *str)
{
	int flags;
	char s;

	// Defaults
	flags = DIRLIST_LONG | DIRLIST_PIPE;


	while( (s = *str++) ) {

		switch( s ) {

		case '-':  // option deliminator, just skip
			break;
		case ' ':  // space not allowed, abort.
			break;
		case 'l':  // long, implies not short, and no xml
			flags |= DIRLIST_LONG;
			flags &= ~(DIRLIST_SHORT|DIRLIST_XML);
			break;
		case '1':  // short, implies no long, but can be xml
			flags |= DIRLIST_SHORT;
			flags &= ~(DIRLIST_LONG);
			break;
		case 'X':  // XML, imples not long, but can be short
			flags |= DIRLIST_XML;
			flags &= ~(DIRLIST_LONG);
			break;
		case 'T':  // temp file, implies no pipe
			flags |= DIRLIST_FILE;
			flags &= ~(DIRLIST_PIPE);
			break;
		case 'P':  // pipe, implies no temp file
			flags |= DIRLIST_PIPE;
			flags &= ~(DIRLIST_FILE);
			break;

			// sorting options.
		case 'N':
			flags |= DIRLIST_SORT_NAME;
			break;
		case 't':
			flags |= DIRLIST_SORT_DATE;
			break;
		case 's':
			flags |= DIRLIST_SORT_SIZE;
			break;
		case 'r':
			flags |= DIRLIST_SORT_REVERSE;
			break;
		case 'C':
			flags |= DIRLIST_SORT_CASE;
			break;
		case 'D':
			flags |= DIRLIST_SORT_DIRFIRST;
			break;

		case 'R':
			flags |= DIRLIST_SHOW_RECURSIVE;
			break;

		case 'a':
			flags |= DIRLIST_SHOW_DOT;
			break;

		case 'W':
			flags |= DIRLIST_SHOW_DIRSIZE;
			break;

		default:
			break;
		}

	}

	return flags;


}




void dirlist_hide_file(char *hideme)
{

	dirlist_child_hide_file( hideme );


}


void dirlist_set_uidlookup( char *(*function)(int uid))
{

	dirlist_child_uid_lookup = function;

}


void dirlist_set_gidlookup( char *(*function)(int gid))
{

	dirlist_child_gid_lookup = function;

}



void dirlist_no_recursion( char *no_recursion )
{

	dirlist_child_no_recursion(no_recursion);

}



//
// If a handle is no more, make sure we don't pass any events as that handle
// if we have outstanding directory listing processing.
//
void dirlist_cancel( lion_t *handle )
{
	dirlist_child_t *node;
	dirlist_queue_t *que;

	for ( node = dirlist_child_head; node; node = node->next ) {

		for ( que = node->queue_head; que ; que = que->next) {

			if (que->user_handle == handle)
				que->user_handle = NULL;

		}

	}

}

