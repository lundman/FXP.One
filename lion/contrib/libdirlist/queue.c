
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

#include "queue.h"





dirlist_queue_t *dirlist_queue_new( dirlist_queue_t **head, dirlist_queue_t **tail )
{
	dirlist_queue_t *que;

	if (!head || !tail)
		return NULL;


	que = (dirlist_queue_t *) malloc(sizeof(*que));
	if (!que) return NULL;
	memset(que, 0, sizeof(*que));

	// Insert at the tail end. if none there, make it us
	// if somethig is there, set its ->next
	if (*tail)
		(*tail)->next = que;

	*tail = que;

	// if head is NULL, this is the new head too!
	if (!*head)
		*head = que;

	return que;
}


dirlist_queue_t *dirlist_queue_pop( dirlist_queue_t **head, dirlist_queue_t **tail )
{
	dirlist_queue_t *que;

	if (!head || !tail)
		return NULL;

	// We take what head is pointing to,
	// and set head to its ->next.

	if (!*head) return NULL; // Empty

	que = *head;
	*head = que->next;  // Might be NULL, so its ok.

	if (*tail == que)
		*tail = que->next;

	return que;
}




void dirlist_queue_free( dirlist_queue_t *node )
{

	if (!node) return;

	SAFE_FREE( node->path );
	SAFE_FREE( node->precat );
	SAFE_FREE( node->stop_msg );
	SAFE_FREE( node );

}

