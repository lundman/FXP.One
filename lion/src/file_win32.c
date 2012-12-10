
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

// $Id: file_win32.c,v 1.6 2008/12/05 01:50:33 lundman Exp $
// File IO functions for the lion library
// Jorgen Lundman January 8th, 2003.


// Universal system headers
#ifdef WIN32


#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <errno.h>
#include <signal.h>
#include <io.h>




// Implementation specific headers

#include "connections.h"
#include "io.h"
#include "lion.h"
#include "sockets.h"
#include "pipe.h"

#include "lion_rcsid.h"


__RCSID("$LiON: lundman/lion/src/file_win32.c,v 1.6 2008/12/05 01:50:33 lundman Exp $");


// All these functions are executed in a different thread. All with have
// a pipe to communicate with parent, and some will have file-descriptors.
//
// The child-thread should:
//   * Close socket when done
//       * NOT close file_socket, but set it to -1. (Must not be freed by child)
//   * No, it appears thread AND main should close file_socket.
//   * Set event to signal we are finished.
//   * Free new node



// ************************************************************************
// ************************************************************************
// ************************************************************************



// Parent: CreatEvent event_start_* event_end_*
// Parent: CreatThread
// Child:  Wait for event_start_*
// Parent: SetEvent event_start (eventually)
// Child:  Finish work
// Parent: Wait for event_end_*
// Child:  SetEvent event_end, exit
// Parent: release lion node.

void file_parameter_handler(const wchar_t* expression,
   const wchar_t* function,
   const wchar_t* file,
   unsigned int line,
   uintptr_t pReserved)
{
#ifdef DEBUG
	printf("[file] parameter handler called..\n");
#endif
}




//
// This is a thread handler to read file IO.
//
// Once started it will wait for a mutex, which is the parent telling
// it when it is allowed to start.
// Then it will enter a loop reading from a file, writing to the pipe.
//
//
int lion_filereader(connection_t *main_node, void *user_data, void *arg)
{
	connection_t *node;
	int temp;
	int red = 0, wrote = 0, ret = 0;

	if (!main_node)
			goto all_done;


	// We allocate a new node here, this places it in "this threads"
	// linked-list. We set the main-thread node to be released anytime.
	node = connections_new();

	connections_dupe(node, main_node); // to, from
	// Release the other, which is in the main threads linked list.
	pipe_release_events(NULL, main_node, main_node);
	main_node->socket = -1;
	main_node->file_fd = -1;
	main_node->pipe_socket = 0;
	main_node->pipe_socket2 = 0;
	main_node->status = ST_DISCONNECT;
	main_node->type = LION_TYPE_NONE;  // set this last.
	main_node = NULL; // No more accesses.


#ifdef DEBUG
	printf("[lion_filereader] waiting permission to start! %p:%x\n",
		   node, node->event_start_in);
#endif

	WaitForSingleObject( node->event_start_in, INFINITE );


#ifdef DEBUG
	printf("[lion_filereader] Starting..%p data %d -> %d\n", node, node->file_fd, node->socket);
#endif

	// This should not be considered by lion_poll(), not that this
	// thread calls it.
	node->status = ST_DISCONNECT;
	node->type = LION_TYPE_PIPE_FILE;


	// Kick pipe into blocking IO again..
	temp = 0;
	ioctlsocket(node->socket, FIONBIO, &temp);
#if 0
	tmpname = strdup("fnXXXXXX");
	tmpname = mktemp(tmpname);

	test = _open(tmpname, O_WRONLY|O_CREAT|O_BINARY, 0644);
#endif
	// In Windows, if the file-descriptor is closed (in main-thread) and we
	// try to use it here, it causes a real exception, and program termination.
	// Instead, we add our own handler so that read() can fail gracefully.
	_set_invalid_parameter_handler(file_parameter_handler);
	while(  ( red = _read(node->file_fd, (char *)node->buffer,
						 node->inbuffer_size) ) > 0) {

		wrote = sockets_write( node->socket, node->buffer, (unsigned int)red );

		if (wrote != red) {
			break;
		}

	}



all_done:
//	close(test);

	if (red == -1) // Error, signal failure.
		ret = errno;
	if (wrote == -1)
		ret = WSAGetLastError();

#ifdef DEBUG
	// Sleeping increases the chance of race bug to kick in
	//Sleep(1000);
#endif

	//close(file_socket);
#ifdef DEBUG
	printf("[file_win32] closed the file: %p status %d (read/write: %d/%d)\n", node, ret,
		red,wrote);
#endif

	// Stop file_socket from being closed here, parent closes it.
	// Socket will be closed in connection_free().
	_close(node->file_fd);
	node->file_fd = -1;
	sockets_close(node->socket);
	node->socket = -1;

#ifdef DEBUG
	printf("[file_win32] telling parent_in we are done with %p\n", node);
#endif
	SetEvent(node->event_end_in);
    pipe_release_events(NULL, NULL, node);
	connections_free(node);
	ExitThread(ret);
	return ret;
}



int lion_filewriter(connection_t *main_node, void *user_data, void *arg)
{
	connection_t *node;
	int temp;
	int red = 0, wrote = 0, ret = 0;

	if (!main_node)
			goto all_done;

	// We allocate a new node here, this places it in "this threads"
	// linked-list. We set the main-thread node to be released anytime.
	node = connections_new();

#ifdef DEBUG
	printf("[file] filewriter %p -> %p\n", main_node, node);
#endif

	connections_dupe(node, main_node); // to, from
	// Release the other, which is in the main threads linked list.
	pipe_release_events(NULL, main_node, main_node);
	main_node->socket = -1;
	main_node->file_fd = -1;
	main_node->pipe_socket = 0;
	main_node->pipe_socket2 = 0;
	main_node->status = ST_DISCONNECT;
	main_node->type = LION_TYPE_NONE;  // set this last.
	main_node = NULL; // No more accesses.


#ifdef DEBUG
	printf("[lion_filewriter] waiting permission to start! %p:%x\n",
		   node, node->event_start_out);
#endif

	WaitForSingleObject( node->event_start_out, INFINITE );

#ifdef DEBUG
	printf("[lion_filereader] starting %p...\n", node);
#endif

	node->status = ST_DISCONNECT;
	node->type = LION_TYPE_PIPE_FILE;

	temp = 0;
	ioctlsocket(node->socket, FIONBIO, &temp);

	_set_invalid_parameter_handler(file_parameter_handler);
	while(  ( red = sockets_read(node->socket, node->obuffer,
								 node->outbuffer_size) ) > 0) {

		wrote = _write( node->file_fd, node->obuffer, red );

		if (wrote != red) {
			break;
		}
	}



all_done:

	if (red == -1) // Error, signal failure.
		ret = WSAGetLastError();

	if (wrote == -1)
		ret = errno;

#ifdef DEBUG
	// Sleeping increases the chance of race bug to kick in
	//Sleep(1);
#endif

	//close(file_socket);
#ifdef DEBUG
	printf("[file_win32] closed the file: %p status %d\n", node, ret);
#endif
	_close(node->file_fd);
	node->file_fd = -1;
	sockets_close(node->socket);
	node->socket = -1;

#ifdef DEBUG
	printf("[file_win32] telling parent_out we are done with %p\n", node);
#endif
	SetEvent(node->event_end_out);
    pipe_release_events(NULL, node, NULL);
	connections_free(node);

	ExitThread(ret);
	return ret;
}



int pipe_child_starter(void *child_pipe)
{
	connection_t *chpipe = (connection_t *) child_pipe;
	connection_t *newd;

	// Sanity checks
	if (!chpipe ||
		!chpipe->start_address ||
		!chpipe->socket)
		_exit(1);

#ifdef DEBUG
	printf("[child_starter] waiting for mutex\n");
#endif

	//if (chpipe->mutex) {
	//	WaitForSingleObject( chpipe->mutex, INFINITE );
	//	CloseHandle( chpipe->mutex );
	//	chpipe->mutex = NULL;
	//}
	if (chpipe->event_start_in) {
		WaitForSingleObject( chpipe->event_start_in, INFINITE );
		ResetEvent(chpipe->event_start_in);
	}

	// We allocate a new node here which goes into "this" thread's
	// linked-list, then we tell the other thread to release its node.

	newd = connections_new();

	connections_dupe(newd, chpipe); // to, from


	// Release the other, which is in the main threads linked list.
	pipe_release_events(NULL, chpipe, chpipe);
	chpipe->socket = -1;
	chpipe->pipe_socket  = 0;
	chpipe->pipe_socket2 = 0;
	chpipe->status = ST_DISCONNECT;
	chpipe->type = LION_TYPE_NONE;


	newd->status = ST_PENDING;  // This way the child gets a connected event.
	newd->type = LION_TYPE_PIPE;

#ifdef DEBUG
	printf("[pipe_child_starter] calling child %p %p\n", chpipe, newd);
#endif


	// We don't know for sure, but we don't stop it being freed at least.
	SetEvent(newd->event_end_in);
	pipe_release_events(NULL, newd, newd);

	ExitThread(newd->start_address(newd, (void *)newd->user_data, NULL));

	return 0; // NOT REACHED, BUT MSVC++ warning avoidance.
}




// Read from lion pipe (from parent) and write out to HANDLE which
// is child-process' stdin.
int lion_stdinwriter(connection_t *main_node, void *user_data, void *arg)
{
	connection_t *node;
	int temp;
	int red = 0, wrote = 0, ret = 0;

	if (!main_node)
			goto all_done;

	// We allocate a new node here, this places it in "this threads"
	// linked-list. We set the main-thread node to be released anytime.
	node = connections_new();

	connections_dupe(node, main_node); // to, from
	// Release the other, which is in the main threads linked list.
	pipe_release_events(NULL, main_node, main_node);
	main_node->socket = -1;
	main_node->pipe_socket  = 0;
	main_node->pipe_socket2 = 0;
	main_node->status = ST_DISCONNECT;
	main_node->type = LION_TYPE_NONE;  // set this last.
	main_node = NULL; // No more accesses.

#ifdef DEBUG
	printf("[pipe_stdinwriter] Waiting to start %p:%x..\n", node,
		   node->event_start_out);
#endif

	WaitForSingleObject( node->event_start_out, INFINITE );

#ifdef DEBUG
	printf("[pipe_stdinwriter] Started %p:%x..\n", node,
		   node->event_start_out);
#endif

	node->status = ST_DISCONNECT;
	node->type = LION_TYPE_PIPE_FILE;

	// Kick pipe into blocking IO again..
	temp = 0;
	ioctlsocket(node->socket, FIONBIO, &temp);


	_set_invalid_parameter_handler(file_parameter_handler);
	while(  ( red = sockets_read(node->socket, node->buffer,
								 node->inbuffer_size) ) > 0) {

		if (!WriteFile(node->pipe_socket, node->buffer, red,
		     &wrote, NULL)) break;

		if (wrote != red) {
			break;
		}
	}



all_done:

#ifdef DEBUG
	// Sleeping increases the chance of race bug to kick in
	//Sleep(1);
#endif

	if (red == -1) // Error, signal failure.
		ret = WSAGetLastError();
	if (wrote == -1)
		ret = errno;
	else
		ret = GetLastError();


	//CloseHandle(node->file_socket);

#ifdef DEBUG
	printf("[pipe_win32] stdinwriter closed the file: status %d\n", ret);
#endif

	//CloseHandle(node->pipe_socket);
	node->pipe_socket = 0;
	shutdown(node->socket, 2);
	sockets_close(node->socket);
	node->socket = -1;

	// Signal parent we are done.
#ifdef DEBUG
	printf("[pipe_win32] telling parent_out we are done with %p\n", node);
#endif
	SetEvent(node->event_end_out);
	pipe_release_events(NULL, node, NULL);
	connections_free(node);

	ExitThread(ret);
	return ret;
}



//
// This is a thread handler to read stdin IO.
//
// Once started it will wait for a mutex, which is the parent telling
// it when it is allowed to start.
// Then it will enter a loop reading from stdin, writing to the pipe.
//
int lion_stdinreader(connection_t *main_node, void *user_data, void *arg)
{
	connection_t *node;
	int temp;
	int red = 0, wrote = 0, ret = 0;

	if (!main_node)
			goto all_done;

	// We allocate a new node here, this places it in "this threads"
	// linked-list. We set the main-thread node to be released anytime.
	node = connections_new();

	connections_dupe(node, main_node); // to, from
	// Release the other, which is in the main threads linked list.
	pipe_release_events(NULL, main_node, main_node);
	main_node->socket = -1;
	main_node->pipe_socket  = 0;
	main_node->pipe_socket2 = 0;
	main_node->status = ST_DISCONNECT;
	main_node->type = LION_TYPE_NONE;  // set this last.
	main_node = NULL; // No more accesses.

#ifdef DEBUG
	printf("[lion_stdinreader] waiting permission to start %p:%x!\n", node,
		   node->event_start_in);
#endif

	WaitForSingleObject( node->event_start_in, INFINITE );

	node->status = ST_DISCONNECT;
	node->type = LION_TYPE_PIPE_FILE;

#ifdef DEBUG
	printf("[lion_stdinreader] Starting.. %p:%x\n", node,
		   node->event_start_in);
#endif
	// Kick pipe into blocking IO again..
	temp = 0;
	ioctlsocket(node->socket, FIONBIO, &temp);

	_set_invalid_parameter_handler(file_parameter_handler);
	while(  ReadFile(node->pipe_socket, node->buffer, node->inbuffer_size,
					 &red, NULL) ) {

		if (red <=0) break;

		wrote = sockets_write( node->socket, node->buffer, red );

		if (wrote != red) {
			break;
		}
	}



all_done:

#ifdef DEBUG
	// Sleeping increases the chance of race bug to kick in
	//Sleep(1);
#endif

	if (!ret)
		ret = GetLastError();
	else
		ret = errno;

	if (wrote == -1)
		ret = WSAGetLastError();


	//CloseHandle(file_socket);
#ifdef DEBUG
	printf("[pipe_win32] closed the file: %p status %d\n", node, ret);
#endif

	//CloseHandle(node->pipe_socket);
	node->pipe_socket = 0;
	shutdown(node->socket, 2);
	sockets_close(node->socket);
	node->socket = -1;

#ifdef DEBUG
	printf("[pipe_win32] telling parent_in we are done with %p\n", node);
#endif
	// Tell parent we are finished
	SetEvent(node->event_end_in);
	pipe_release_events(NULL, NULL, node);
	connections_free(node);

	ExitThread(ret);
	return ret;
}



#endif
