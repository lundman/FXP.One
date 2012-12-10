
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

// $Id: connections.c,v 1.7 2010/10/26 06:38:40 lundman Exp $
// Temporary connection space holder
// Jorgen Lundman November 5th, 1999


// As we get new connections, we need a few values and information to be
// stored with them. This is an example on how this could be achieved.
// Currently it is a simple linked list of nodes, one for each connection.
// Eventually, this with belong to a high connection database information
// module.

// This module is more complicated to the casual reader than it really needs
// to be, but that is so that it can be as modular as possible.
// Although if you really want to achieve this, the linked list would have
// just nodes of "next" and "data" (void *) totally hiding data from this
// layer, as well as, hiding the implementation from the data-layer.

#include <stdio.h>        // for defn of NULL
#include <stdlib.h>       // malloc
#include <string.h>       // memset
#include <signal.h>
#include <time.h>

#include "connections.h"
//#include "misc.h"

// The start to the linked list, it's static as only we may touch it.
THREAD_SAFE static connection_t *connections_head = NULL;


// The main buffer size variable, so that we can change it on load.
unsigned int buffer_size = BUFFER_SIZE_DEFAULT;



#include "lion.h" // Only for DEBUG define!!
#include "pipe.h"


//
// Allocate and return a fresh Connection Holder.
//
// IN:  void
// OUT: pointer to a new node, added to the main linked list.
//
connection_t *connections_new(void)
{
  connection_t *result;

  //	printf("  head is at %p pointing to %p\n", &connections_head, connections_head);

  result = (connection_t *) malloc(sizeof (connection_t));

  if (!result) {

    perror("connections_new: malloc:");
    exit(1);       // FIXME.

  }

#ifdef DEBUG_VERBOSE
  printf("Allocated new Connection %p\n", result);
#endif

  // clear it
  memset(result, 0, sizeof(*result));
  //result->trace=1;

  // Add it to linked list
  result->next = connections_head;
  connections_head = result;


  // Allocate input buffer
  result->buffer = (char *) malloc(buffer_size);

  if (!result->buffer) {

    perror("connections_new: buffer area malloc:");
    exit(1);       // FIXME.

  }


  // Allocate output buffer
  result->obuffer = (char *) malloc(buffer_size);

  if (!result->obuffer) {

    perror("connections_new: buffer area malloc:");
    exit(1);       // FIXME.

  }

  // Clear the buffers to avoid UMR
  memset(result->buffer, 0, buffer_size);
  memset(result->obuffer, 0, buffer_size);

  result->inbuffer_size = buffer_size;
  result->outbuffer_size = buffer_size;


  result->return_code = -1;  // -1 == unknown


  // That's all, return it.
  return result;

}


//
// Free a connection node, and remove it from the linked list.
//
// IN:  connection ptr to be freed
// OUT: void
//
void connections_free(connection_t *node)
{
  connection_t *runner, *prev;

  /* assert(!node); */
  if (node->trace)
	  fprintf(trace_file, "%p: connections_free (node released)\n", node);


  if (node->trace)
	  lion_disable_trace(node);

#ifdef WIN32
  // If we have event set, for the threads, we need to wait until they are
  // set.
  pipe_release_events(node, NULL, NULL);

  if (node->process) {
	  int i;

	  i = TerminateProcess(node->process, 123);

#ifdef DEBUG
	  printf("[connections_free] attempted to kill thread %d pid %d: %d (1 = killed)\n",
			 node->process, node->pid);
#endif
	  CloseHandle((HANDLE)node->process);
	  node->process = NULL;
  }

		if (node->thread) {
#ifdef DEBUG
			printf("[io] closing thread %p:%d\n", node, node->thread);
#endif
			CloseHandle( node->thread );
			node->thread = NULL;
		}

		if (node->thread2) {
#ifdef DEBUG
			printf("[io] closing thread2 %p:%d\n", node, node->thread2);
#endif
			CloseHandle( node->thread2 );
			node->thread2 = NULL;
		}
		if (node->pipe_socket>0) {
			CloseHandle(node->pipe_socket);
			node->pipe_socket = 0;
		}
		if (node->pipe_socket2>0) {
			CloseHandle(node->pipe_socket2);
			node->pipe_socket2 = 0;
		}

#else // Unix

  if (node->pid) {
	  int i;
	  i = kill(SIGKILL, node->pid);

#ifdef DEBUG
#ifdef WIN32
	  printf("[connections_free] attempted to kill process %d pid %d: %d (0 = killed)\n",
			 node->thread, node->pid);
#endif
#endif

	  node->pid = 0;
  }
#endif


  // Free the buffer if it was allocated

  if (node->buffer) {

    free( node->buffer );
    node->buffer = NULL;

  }

  if (node->obuffer) {

    free( node->obuffer );
    node->obuffer = NULL;

  }

#ifdef DEBUG_VERBOSE
  printf("[releasing input/output buffers size %d / %d]\n",
		 node->inbuffer_size,
		 node->outbuffer_size);
#endif


  for (prev = NULL, runner = connections_head;
       runner;
       prev = runner, runner = runner->next) {


	  if (runner == node) {  // This is the one to remove

		  if (!prev) {

			  // If no previous node, then it's at the start of the list

			  connections_head = runner->next;

		  } else {

			  // In the middle somewhere

			  prev->next = runner->next;

		  }

		  break;  // Stop spinning in the for-loop.

	  } // if runner == node

  } // for


#ifdef DEBUG_VERBOSE
  printf("Releasing %p\n", node);
#endif

#ifdef DEBUG
  memset(node, -1, sizeof(*node));
#endif


  // Release it
  free( node );

}





//
// Find a particular node, or, iterate the list.
//
// IN:  comparison function, expected to return 0 on match
// IN:  two optional arguments that are simply passed on
// OUT: pointer to matching node, or NULL if end reached
//
connection_t *connections_find( int (*compare)(connection_t *, void *, void *),
				void *optarg1, void *optarg2)
{
  connection_t *runner;

  for (runner = connections_head; runner; runner = runner->next) {

    if (!compare(runner, optarg1, optarg2)) {

      // Found it, apparently
      return runner;

    }

  }

  // Didn't find it
  return NULL;

}





void connections_dupe(connection_t *dst, connection_t *src)
{
	// Copy over anything we need.
#define NODE_DUPE(X) dst->X = src->X
	NODE_DUPE(type);
	NODE_DUPE(socket);
	NODE_DUPE(status);
	NODE_DUPE(binary);
	NODE_DUPE(disable_read);
	NODE_DUPE(user_data);
	NODE_DUPE(rate_in);
	NODE_DUPE(rate_out);
	NODE_DUPE(time_start);
	NODE_DUPE(event_handler);
	NODE_DUPE(trace);
#ifdef WIN32
	//NODE_DUPE(mutex);
	NODE_DUPE(event_start_in);
	NODE_DUPE(event_start_out);
	NODE_DUPE(event_end_in);
	NODE_DUPE(event_end_out);
	NODE_DUPE(file_fd);
	NODE_DUPE(pipe_socket);
	NODE_DUPE(pipe_socket2);
	NODE_DUPE(start_address);
#endif
	// More to come
#undef NODE_DUPE
}


void connections_cycle( void )
{
	connection_t *runner;

	for (runner = connections_head; runner; runner = runner->next) {

		if (!runner->next) {  // Last node.

			if ( connections_head != runner ) { // Not the only thing in list.

				// Find the last node, assign it to have a ->next pointing to
				// the first node in the list.
				runner->next = connections_head;

				// Then we need to save the first node's ->next to re assign
				runner = connections_head->next;

				// Set first node (now last) ->next to be NULL, EOL.
				connections_head->next = NULL;

				// Re-assign the front of the list to point to 2nd node.
				connections_head = runner;

				// and stop
				return;

			}

		}

	}

}


void connections_dump(void)
{
	connection_t *node;

	printf("Connections_dump: \n");

	for (node = connections_head; node; node = node->next) {
		printf("%p: type: %02d status: %02d handler: %p disable_read: %d want_mode %d\n",
			node, node->type, node->status, node->event_handler, node->disable_read, node->want_mode);
		printf("%p: soft_closed: %d inbuffer: %d outbuffer %d user_data: %p socket: %d\n",
			node, node->soft_closed, node->inbuffer, node->outbuffer, node->user_data, node->socket);
#ifdef WIN32
		printf("%p: file_fd: %04d pipe_socket: %04d pipe_socket2: %04d thread: %p thread2: %p\n",
			node, node->file_fd, node->pipe_socket, node->pipe_socket2, node->thread, node->thread2);
#endif
		printf("\n");
	}
}

