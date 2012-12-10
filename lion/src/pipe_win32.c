
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

// $Id: pipe_win32.c,v 1.13 2011/06/13 06:36:22 lundman Exp $
// Win32 specific solutions to help with pipes.
// Jorgen Lundman January 29th, 2003.

#ifdef WIN32

// Universal system headers
#define _BSD_SIGNALS
#include <stdio.h>
#define WINDOWS_LEAN_AND_MEAN
#include <winsock2.h>
#include <io.h>
#include <errno.h>
#include <direct.h>
#include <share.h>

#include "connections.h"
#include "lion.h"
#include "io.h"
#include "pipe.h"
#include "sockets.h"
#include "misc.h"


// Right, here's the deal. We want to handle sockets, and files with one
// place of sleep/cpu-release. to be maximum efficient.
//
// But Windows don't allow file handles to be added to select().
// So we try WaitForMultipleObjects, and Overlapped IO
// but Win 95, 98 and ME don't handle Overlapped IO on disk files.
// There is no fcntl for nonblocking under windows.
// Right, seems the answer in Windows is to thread! Rather inefficient
// but if a user is running Windows they can't be all that concerned about
// efficiency.
// So we thread and create a pipe. But anonymous pipes are uni-directional.
// So we have to create a named pipe.
// Named pipes are not supported under Win95, 98 and ME.
// So, lets call socketpair instead.
// There is no socketpair under winsock.
// So lets manually create a socketpair, thread off to deal with
// file IO to is just written/read to pipe.
// If you open file O_RDWR it has to start two threads!
//
// And people think I'm biased because I love developing under Unix.
//
// Cos I'm assing tired of this, it is implemented in BLOCKING mode
// as it happens localhost connects are instant anyway, But in future
// this should be change to an internal thing, using lions own
// calls to set it all up
//

//
// Perform a fork(). The child gets NULL returned to it.
// If there is a failure, a valid node is still issues, and
// the _LOST event is sent. If the child actually needs to
// get a handle as well, it can pass the "child" ptrptr.
//

//static FILE *fd = NULL;


__RCSID("$LiON: lundman/lion/src/pipe_win32.c,v 1.13 2011/06/13 06:36:22 lundman Exp $");



int pipe_make_events(connection_t *parent_pipe, connection_t *child_out,
					 connection_t *child_in)
{

	if (child_in) {
		parent_pipe->event_start_in  = CreateEvent( NULL, TRUE, FALSE, NULL );
		parent_pipe->event_end_in    = CreateEvent( NULL, TRUE, FALSE, NULL );
		if (!parent_pipe->event_start_in || !parent_pipe->event_end_in)
			return 0;
#ifdef DEBUG
		printf("[pipe_win32] created events_in %p\n", parent_pipe);
#endif
		child_in->event_start_in  = parent_pipe->event_start_in;
		child_in->event_end_in    = parent_pipe->event_end_in;
	}

	if (child_out) {
		parent_pipe->event_start_out = CreateEvent( NULL, TRUE, FALSE, NULL );
		parent_pipe->event_end_out   = CreateEvent( NULL, TRUE, FALSE, NULL );
		if (!parent_pipe->event_start_out || !parent_pipe->event_end_out)
			return 0;
#ifdef DEBUG
		printf("[pipe_win32] created events_out %p\n", parent_pipe);
#endif
		child_out->event_start_out = parent_pipe->event_start_out;
		child_out->event_end_out   = parent_pipe->event_end_out;
	}

	return 1;
}


//
// Only called on failures
//
void pipe_release_events(connection_t *parent_pipe, connection_t *child_out,
						 connection_t *child_in)
{

	if (parent_pipe) {
#ifdef DEBUG
		printf("[pipe_win32] releasing parent events %p (%x,%x,%x,%x)\n",
			   parent_pipe,
			   parent_pipe->event_start_in,
			   parent_pipe->event_start_out,
			   parent_pipe->event_end_in,
			   parent_pipe->event_end_out);
#endif
		if (parent_pipe->event_start_in)
			CloseHandle(parent_pipe->event_start_in);
		if (parent_pipe->event_start_out)
			CloseHandle(parent_pipe->event_start_out);
		if (parent_pipe->event_end_in)
			CloseHandle(parent_pipe->event_end_in);
		if (parent_pipe->event_end_out)
			CloseHandle(parent_pipe->event_end_out);
		parent_pipe->event_start_in  = 0;
		parent_pipe->event_start_out = 0;
		parent_pipe->event_end_in    = 0;
		parent_pipe->event_end_out   = 0;
	}

	if (child_out) {
	child_out->event_start_out = 0;
	child_out->event_end_out   = 0;
#ifdef DEBUG
	printf("[pipe_win32] clearing child_out events %p\n", child_out);
#endif
	}
	if (child_in) {
		child_in->event_start_in  = 0;
		child_in->event_end_in    = 0;
#ifdef DEBUG
		printf("[pipe_win32] clearing child_in events %p\n", child_in);
#endif
	}
}



connection_t *lion_filethread(int file,
						 int (*start_address)(connection_t *, void *,void *),
							  void *arg, void *user_data, int is_in_direction)
{
	connection_t *parent_pipe, *child_pipe;
	SECURITY_ATTRIBUTES sec;
	HANDLE shared_mutex = NULL;

	parent_pipe = connections_new();

	parent_pipe->type = LION_TYPE_PIPE_FILE;
	parent_pipe->user_data = user_data;
	parent_pipe->file_fd = file;

	child_pipe = connections_new();

	child_pipe->type = LION_TYPE_PIPE;
	//child_pipe->type = LION_TYPE_THREAD_NODE;
	child_pipe->user_data = user_data; // child should call set_userdata.
	child_pipe->file_fd = file;


	// Connect the two nodes.
	if (pipe_make_pipes(parent_pipe, child_pipe))
		goto failed;
#ifdef DEBUG
	printf("[thread] making pipes; parent %p, child %p (%d/%d)\n", parent_pipe, child_pipe,
		parent_pipe->socket, child_pipe->socket);
#endif



	// Unfortunately, we need a mutex to stop the child from running right away.

	sec.nLength = sizeof(sec);
	sec.lpSecurityDescriptor = NULL;
	sec.bInheritHandle = TRUE;

#ifdef DEBUG
	printf("[thread] creating mutex\n");
#endif
	// Create the mutex to stop the child from running.
	//shared_mutex = CreateMutex( &sec, TRUE, NULL );
	//                  secLP, ManualRst, InitState, lpName

	if (is_in_direction) {
		if (!pipe_make_events(parent_pipe, NULL, child_pipe))
			goto failed;
	} else {
		if (!pipe_make_events(parent_pipe, child_pipe, NULL))
			goto failed;
	}

	// We don't want to consider the child node as parent anymore
	// The child sets this to disconnect when ready.
	// This would free this node far too soon.
	//child_pipe->type = LION_TYPE_NONE;
	lion_disable_read(child_pipe);


#ifdef DEBUG
	printf("[thread] creating thread\n");
#endif

  parent_pipe->thread = CreateThread(NULL, 0,
									   (LPTHREAD_START_ROUTINE) start_address,
									   (void *) child_pipe,0, NULL);


	if ( parent_pipe->thread == NULL ) {
		goto failed;
	}



	// Parent!

	// We need to release the "child_pipe" on this side as we are parent.
	// and this node no longer exists on this end.


#ifdef DEBUG
	printf("[parent] setting pending. %p\n", parent_pipe);
#endif

	// since we may or may not have data for select, force a loop to
	// send event.
	io_force_loop = 1;

	parent_pipe->status = ST_PENDING;
	return parent_pipe;



 failed:
	// We failed. Set parent status to DISCONNECT, and release the child.
	// lnet will then issue _LOST to application
	child_pipe->status = ST_DISCONNECT;
	lion_disconnect(child_pipe);

	parent_pipe->status = ST_PENDING;
	if (parent_pipe->socket > 0)
		sockets_close(parent_pipe->socket);
	parent_pipe->socket = -1;   // This will notify it of failure, WAIT? Who closes parent pipe?

	//	if (shared_mutex) {
	//	CloseHandle(shared_mutex);
	//	parent_pipe->mutex = NULL;
	//	child_pipe->mutex = NULL;
	//
	// If we already set up events, free them, and clear them.
	pipe_release_events(parent_pipe, NULL, child_pipe);

	_close( file );
	// We want select to poll next iteration and enter processing
	// as we don't have a real fd to add to select, but we need to send
	// events from that part of the library
	io_force_loop = 1;

	return NULL;

}






connection_t *lion_filethread2(HANDLE file1, HANDLE file2,
							  int (*start_address1)(connection_t *, void *, void *),
							  int (*start_address2)(connection_t *, void *, void *), void *user_data)
{
	connection_t *parent_pipe, *child_pipe1, *child_pipe2;
	SECURITY_ATTRIBUTES sec;
	HANDLE shared_mutex = NULL;

	parent_pipe = connections_new();

	parent_pipe->type = LION_TYPE_PIPE_FILE;
	parent_pipe->user_data = user_data;
	parent_pipe->pipe_socket = file1;
	//parent_pipe->pipe_socket2 = file2;

	child_pipe1 = connections_new();
	child_pipe1->type = LION_TYPE_PIPE;
	//child_pipe1->type = LION_TYPE_THREAD_NODE;
	child_pipe1->user_data = user_data; // child should call set_userdata.
	child_pipe1->pipe_socket = file1;

	child_pipe2 = connections_new();
	child_pipe2->type = LION_TYPE_PIPE;
	//child_pipe2->type = LION_TYPE_THREAD_NODE;
	child_pipe2->user_data = user_data; // child should call set_userdata.
	child_pipe2->pipe_socket = file2;


#ifdef DEBUG
	printf("[thread] making pipes\n");
#endif
	// Connect the two nodes.
	if (pipe_make_pipes(parent_pipe, child_pipe1))
		goto failed;

	child_pipe2->socket = child_pipe1->socket;

#ifdef DEBUG
    printf("[pipe32] child_pipe1 %p:%u, child_pipe2 %p:%u\n",
           child_pipe1, child_pipe1->socket,
           child_pipe2, child_pipe2->socket);
#endif

	// Unfortunately, we need a mutex to stop the child from running right away.

	sec.nLength = sizeof(sec);
	sec.lpSecurityDescriptor = NULL;
	sec.bInheritHandle = TRUE;

#ifdef DEBUG
	printf("[thread] creating mutex\n");
#endif
	// Create the mutex to stop the child from running.
	//shared_mutex = CreateMutex( &sec, TRUE, NULL );

	//if (shared_mutex == NULL)
	//	goto failed;

	//child_pipe2->mutex = shared_mutex;
	//parent_pipe->mutex = shared_mutex;


	if (!pipe_make_events(parent_pipe, child_pipe1, child_pipe2))
		goto failed;


	// We don't want to consider the child node as parent anymore
	// The child sets this to disconnect when ready.
	// This would free this node far too soon.
	//child_pipe->type = LION_TYPE_NONE;
	lion_disable_read(child_pipe1);
	lion_disable_read(child_pipe2);


#ifdef DEBUG
	printf("[thread] creating thread\n");
#endif

	SetHandleInformation( file1, HANDLE_FLAG_INHERIT, 1);
	parent_pipe->thread = CreateThread(NULL, 0,
								   (LPTHREAD_START_ROUTINE) start_address1,
									   (void *) child_pipe1,0, NULL);


	if ( parent_pipe->thread == NULL ) {
		goto failed;
	}

	SetHandleInformation( file1, HANDLE_FLAG_INHERIT, 0);
	SetHandleInformation( file2, HANDLE_FLAG_INHERIT, 1);
	parent_pipe->thread2 = CreateThread(NULL, 0,
								   (LPTHREAD_START_ROUTINE) start_address2,
									   (void *) child_pipe2,0, NULL);


	if ( parent_pipe->thread2 == NULL ) {
		goto failed;
	}

#ifdef DEBUG
	printf("[pipe] *** thread2 is %p:%d\n", parent_pipe, parent_pipe->thread2);
#endif
	SetHandleInformation( file2, HANDLE_FLAG_INHERIT, 0);


	// Parent!

	// We need to release the "child_pipe" on this side as we are parent.
	// and this node no longer exists on this end.


#ifdef DEBUG
	printf("[parent] setting pending. %p\n", parent_pipe);
#endif

	// since we may or may not have data for select, force a loop to
	// send event.
	io_force_loop = 1;

	parent_pipe->status = ST_PENDING;
	return parent_pipe;



 failed:
	// We failed. Set parent status to DISCONNECT, and release the child.
	// lnet will then issue _LOST to application
	child_pipe1->status = ST_DISCONNECT;
	lion_disconnect(child_pipe1);
	child_pipe2->status = ST_DISCONNECT;
	lion_disconnect(child_pipe2);

	parent_pipe->status = ST_PENDING;
	if (parent_pipe->socket > 0)
		sockets_close(parent_pipe->socket);
	parent_pipe->socket = -1;   // This will notify it of failure, WAIT? Who closes parent pipe?

	//	if (shared_mutex) {
	//	CloseHandle(shared_mutex);
	//	parent_pipe->mutex = NULL;
	//	child_pipe1->mutex = NULL;
	//	child_pipe2->mutex = NULL;
	//	}
	pipe_release_events(parent_pipe, child_pipe1, child_pipe2);

	CloseHandle( file1 );
	CloseHandle( file2 );
	// We want select to poll next iteration and enter processing
	// as we don't have a real fd to add to select, but we need to send
	// events from that part of the library
	io_force_loop = 1;

	return NULL;

}





connection_t *lion_open(char *file, int flags, mode_t modes,
						int lion_flags, void *user_data)
{
	connection_t *newd, *parent = NULL;

	// This breaks FULLFILL, but then, passing NULL breaks lion API.
	if (!file)
		return NULL;

	newd = connections_new();

	newd->type = LION_TYPE_FILE;
	newd->user_data = user_data;

	if (lion_flags & LION_FLAG_TRACE)
		lion_enable_trace(newd);

	if (newd->trace)
		fprintf(trace_file, "%p: lion_open(%s)\n", newd, file);

	// Open the file, if issues, send event.

	if (lion_flags & LION_FLAG_EXCLUSIVE) {

		newd->file_fd = _sopen(file,
							 flags | O_BINARY,
							 _SH_DENYWR,
							 modes);

	} else {

		newd->file_fd = _open( file, flags | O_BINARY, modes );

	}

#ifdef DEBUG
	if (newd->file_fd < 0)
		perror("open");
	printf("lion_open(%s): %d\n", file, newd->file_fd);
#endif

	if (newd->file_fd < 0) {

		if (lion_flags & LION_FLAG_FULFILL) {

			// Signal above layer of failure
			newd->status = ST_PENDING;

			io_force_loop = 1;
			return newd;
		}

		newd->status = ST_DISCONNECT;

		return NULL;
	}



	// So we now know the file was opened successfully. We now start
	// of the thread(s) based on file open modes.
	// But we have TWO nodes now, One pure FILE that has the fd.
	// and parent gets one end of the pipe.
	if (flags & O_RDWR) {  // Worst case


	} else if (flags & O_WRONLY) {

		parent = lion_filethread(newd->file_fd, lion_filewriter, NULL, user_data, 0);

	} else {

		parent = lion_filethread(newd->file_fd, lion_filereader, NULL, user_data, 1);


	}


	if (!parent) {
		newd->status = ST_PENDING;
		_close(newd->file_fd);
		newd->file_fd = -1;
		return NULL;
	}


	// "newd" is a temporary node, only used in case of failure
	// "parent" is the node to use.
	parent->file_fd = newd->file_fd;
	if (lion_flags & LION_FLAG_TRACE)
		lion_enable_trace(parent);

	newd->socket = -1;
	newd->file_fd = -1;
	newd->status = ST_DISCONNECT;  // will be released eventually.

#ifdef DEBUG
	printf("[file] lion_open(%s) %p -> %p\n", file, newd, parent);
#endif

	return parent;

}



connection_t *lion_fork(int (*start_address)(connection_t *, void *, void *),
						int flags, void *user_data, void *arg)
{
	connection_t *parent_pipe, *child_pipe;
	SECURITY_ATTRIBUTES sec;
	HANDLE shared_mutex = NULL;

	parent_pipe = connections_new();

	parent_pipe->type = LION_TYPE_PIPE;
	parent_pipe->user_data = user_data;

	if (flags & LION_FLAG_TRACE)
		lion_enable_trace(parent_pipe);

	if (parent_pipe->trace)
		fprintf(trace_file, "%p: lion_fork\n", parent_pipe);

	child_pipe = connections_new();

	child_pipe->type = LION_TYPE_PIPE;
	//child_pipe->type = LION_TYPE_THREAD_NODE;
	child_pipe->user_data = user_data; // child should call set_userdata.

#ifdef DEBUG
	printf("[thread] making pipes\n");
#endif
	// Connect the two nodes.
	if (pipe_make_pipes(parent_pipe, child_pipe))
		goto failed;


	// Unfortunately, we need a mutex to stop the child from running right away.

	sec.nLength = sizeof(sec);
	sec.lpSecurityDescriptor = NULL;
	sec.bInheritHandle = TRUE;

#ifdef DEBUG
	printf("[thread] creating mutex\n");
#endif
	// Create the mutex to stop the child from running.
	//shared_mutex = CreateMutex( &sec, TRUE, NULL );

	//if (shared_mutex == NULL)
	//	goto failed;

	//child_pipe->mutex = shared_mutex;
	//parent_pipe->mutex = shared_mutex;

	if (!pipe_make_events(parent_pipe, NULL, child_pipe))
		goto failed;


	// We don't want to consider the child node as parent anymore
	// The child sets this to disconnect when ready.
	child_pipe->type = LION_TYPE_NONE;
	//child_pipe->type = LION_TYPE_THREAD_NODE;
	child_pipe->start_address = start_address;

#ifdef DEBUG
	printf("[thread] creating thread\n");
#endif

	parent_pipe->thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE) pipe_child_starter, (void *) child_pipe,0, NULL); //&a_threadId);


	if ( parent_pipe->thread == NULL ) {
		goto failed;
	}



	// Parent!

	// We need to release the "child_pipe" on this side as we are parent.
	// and this node no longer exists on this end.


#ifdef DEBUG
	printf("[parent] setting pending. %p\n", parent_pipe);
#endif

	// since we may or may not have data for select, force a loop to
	// send event.
	io_force_loop = 1;

	parent_pipe->status = ST_PENDING;

	return parent_pipe;



 failed:

	// We failed, so set the reason here, so we can give it to the application
	parent_pipe->error_code = errno;

	// We failed. Set parent status to DISCONNECT, and release the child.
	// lnet will then issue _LOST to application
	child_pipe->status = ST_DISCONNECT;
	lion_disconnect(child_pipe);

	parent_pipe->status = ST_PENDING;
	if (parent_pipe->socket > 0)
		sockets_close(parent_pipe->socket);
	parent_pipe->socket = -1;

	//if (shared_mutex) {
	//	CloseHandle(shared_mutex);
	//	parent_pipe->mutex = NULL;
	//	child_pipe->mutex = NULL;
	//}

	pipe_release_events(parent_pipe, NULL, child_pipe);

	// We want select to poll next iteration and enter processing
	// as we don't have a real fd to add to select, but we need to send
	// events from that part of the library
	io_force_loop = 1;

	return NULL;

}


//
// Execute a new process. Unfortunately in windows you can not
// give a SOCKET as your STD_INPUT_HANDLE, as that takes a
// HANDLE. So here we need to create a "lion pipe" for the
// parent end. And two uni-directional pipes to be STDIN and
// STDOUT. Then start two threads:
// parent_pipe  ->  stdinWr  ->   stdin
// parent_pipe  <-  stdoutRd <-   stdout
//
connection_t *lion_execve2(char *base, char **argv, char **envp,
						  int with_stderr, int lion_flags, void *user_data)
{
	connection_t *parent_pipe, *child_pipe;
	SECURITY_ATTRIBUTES sec;
	STARTUPINFO    si;
	PROCESS_INFORMATION  pi;
    SECURITY_ATTRIBUTES sa;
    int i;

	parent_pipe = connections_new();

	parent_pipe->type = LION_TYPE_PIPE;
	parent_pipe->user_data = user_data;

	if (lion_flags & LION_FLAG_TRACE)
		lion_enable_trace(parent_pipe);

	child_pipe = connections_new();

	child_pipe->type = LION_TYPE_PIPE;
	child_pipe->user_data = user_data; // child should call set_userdata.

#ifdef DEBUG
	printf("[execv] making pipes\n");
#endif

	// Connect the two nodes. This is the LION pipe.
	if (pipe_make_pipes(parent_pipe, child_pipe))
		goto failed;

	// Setup Security for CreatePipe.
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);     // Size of struct
    sa.lpSecurityDescriptor = NULL;               // Default
    sa.bInheritHandle = TRUE;                    // Inheritable


	// Get ready to start the Process
	sec.nLength = sizeof(sec);
	sec.lpSecurityDescriptor = NULL;
	sec.bInheritHandle = TRUE;

	GetStartupInfo(&si);
    si.dwFlags    = STARTF_USESTDHANDLES;

	si.hStdInput  = child_pipe;
	si.hStdOutput = child_pipe;
    si.hStdError  = with_stderr ? child_pipe : GetStdHandle(STD_ERROR_HANDLE);


	// We don't want to consider the child node as parent anymore
	// The child sets this to disconnect when ready.

#ifdef DEBUG
	printf("[execv] 2 creating process\n");
#endif

    // Let's try to create the process up to 10 times, because windows
    // likes to fail sometimes.
    for (i = 0; i < 10; i++) {

        if (CreateProcess(
                          NULL,
                          argv[0],            // command line
                          NULL,            // process security
                          NULL,            // thread security
                          TRUE,            // inherit handles-yes
#ifdef DEBUG
                          0,
#else
                          CREATE_NO_WINDOW,               // creation flags
#endif
                          envp,            // environment block
                          NULL,            // current directory
                          &si,             // startup info
                          &pi))            // process info (out)
            {
                break; // Suuccess, leave loop
            }

        Sleep(100);

    }

    // If "i" is ZERO, it worked first time, so say nothing about it
    // Otherwise, print a message to the user.
    if (i) {

        if (i >= 10) goto failed;

        printf("\n[pipe] NOTE: It took %d calls to CreateProcess to get it starting..\n", i);

    }


    // The Windows example closes these two HANDLES
    //CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);


	parent_pipe->process = pi.hProcess;
	parent_pipe->pid = pi.dwProcessId;

#ifdef DEBUG
	printf("[pipe] process %p:%d\n", parent_pipe, parent_pipe->process);
#endif

	// Parent!
	child_pipe->socket = -1; // The new child closes it
	child_pipe->type = LION_TYPE_NONE;
	child_pipe->status = ST_DISCONNECT;
	child_pipe->pipe_socket = 0;
	child_pipe->pipe_socket2 = 0;



	// We need to release the "child_pipe" on this side as we are parent.
	// and this node no longer exists on this end.


#ifdef DEBUG
	printf("[parent] setting pending. %p\n", parent_pipe);
#endif

	// since we may or may not have data for select, force a loop to
	// send event.
	io_force_loop = 1;

	parent_pipe->status = ST_PENDING;

	return parent_pipe;



 failed:

    //#ifdef DEBUG
	printf("[execv] failed %d/%d\n", errno, GetLastError());
    //#endif

	// We failed. Set parent status to DISCONNECT, and release the child.
	// lnet will then issue _LOST to application

	if (lion_flags & LION_FLAG_FULFILL) {

		//child_pipe->status = ST_DISCONNECT;
		// Close the actual socket, and set fd to -1, that way
		// we will send a FAILED event to the application.
		sockets_close(child_pipe->socket);
		child_pipe->socket = -1;


                //child_pipe->type = LION_TYPE_PIPE;
                child_pipe->error_code = ENOENT;
                child_pipe->socket = -1;
                child_pipe->status = ST_PENDING;



		// Parent side we just need to clean up quietly.
		parent_pipe->status = ST_PENDING;
		if (parent_pipe->socket > 0)
			sockets_close(parent_pipe->socket);
		parent_pipe->socket = -1;   // This will notify it of failure, WAIT? Who closes parent pipe?

		io_force_loop = 1;

		return child_pipe;
	}


	child_pipe->status = ST_DISCONNECT;
	lion_disconnect(child_pipe);

	parent_pipe->status = ST_PENDING;
	if (parent_pipe->socket > 0)
		sockets_close(parent_pipe->socket);
	parent_pipe->socket = -1;   // This will notify it of failure, WAIT? Who closes parent pipe?






	// We want select to poll next iteration and enter processing
	// as we don't have a real fd to add to select, but we need to send
	// events from that part of the library
	io_force_loop = 1;

	return NULL;

}






connection_t *lion_execve(char *base, char **argv, char **envp,
						  int with_stderr, int lion_flags, void *user_data)
{
	connection_t *parent_pipe = NULL, *child_pipe = NULL, *newd = NULL;
	SECURITY_ATTRIBUTES sec;
	STARTUPINFO    si;
	PROCESS_INFORMATION  pi;
    SECURITY_ATTRIBUTES sa;
    HANDLE hChildStdinRd = 0, hChildStdinWr = 0,
           hChildStdoutRd = 0, hChildStdoutWr = 0;
	int i, len = 0, retry;
	char *cmd = NULL;

	//fd = fopen("c:/tmp/yes.txt", "a");
	//if (fd) { fprintf(fd, "execve started '%s' '%s'\r\n", argv[0], argv[1]); fflush(fd);}

	// Windows CreateProcess does not take argv vector, so we need to make
	// it into a string again, so we are POSIX compliant.
	for (i = 0; argv[i]; i++) {
        // Does this section have spaces? if so, use ""
        if (strchr(argv[i], ' ') )
            len += strlen(argv[i]) + 3;  // " AND " AND +space OR +null
        else
            len += strlen(argv[i]) + 1;  // +space OR +null
    }

	if (len) {
		cmd = (char *) malloc(len);
		if (!cmd) goto failed;
		*cmd = 0;
		for (i = 0; argv[i]; i++) {

            // Spaces in this part? if so, use ""
            if (strchr(argv[i], ' ') )
                strcat(cmd, "\"");
			strcat(cmd, argv[i]);
            if (strchr(argv[i], ' ') )
                strcat(cmd, "\"");

			if (argv[i+1])
				strcat(cmd, " ");
		}
	} else
		cmd = _strdup(base);
	if (!cmd) goto failed;

	//if (fd) { fprintf(fd, "base '%s'  cmd '%s'\r\n", base, cmd); fflush(fd);}



	newd = connections_new();

	newd->type = LION_TYPE_PIPE;
	newd->user_data = user_data;

	if (lion_flags & LION_FLAG_TRACE)
		lion_enable_trace(newd);

	if (newd->trace)
		fprintf(trace_file, "%p: lion_execv(%s)\n", base);


	// Setup Security for CreatePipe.
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);     // Size of struct
    sa.lpSecurityDescriptor = NULL;               // Default
    sa.bInheritHandle = TRUE;                    // Inheritable

	// Create STDIN and STDOUT
    if (! CreatePipe(&hChildStdoutRd, &hChildStdoutWr, &sa, 0))
        goto failed;

	if (! CreatePipe(&hChildStdinRd, &hChildStdinWr, &sa, 0))
        goto failed;

    //si.hStdError  = with_stderr ? hChildStdoutWr : GetStdHandle(STD_ERROR_HANDLE);

	// We don't want to consider the child node as parent anymore
	SetHandleInformation( hChildStdoutWr, HANDLE_FLAG_INHERIT, 0);
	SetHandleInformation( hChildStdinRd, HANDLE_FLAG_INHERIT, 0);
	SetHandleInformation( hChildStdinWr, HANDLE_FLAG_INHERIT, 0);
	SetHandleInformation( hChildStdoutRd, HANDLE_FLAG_INHERIT, 0);

	parent_pipe = lion_filethread2(hChildStdinWr,
								   hChildStdoutRd,
								   lion_stdinwriter,
								   lion_stdinreader,
								   user_data);
	if (!parent_pipe) goto failed;

	//CloseHandle(hChildStdinWr);
	//CloseHandle(hChildStdoutRd);

#ifdef DEBUG
	printf("[execv] 1 creating process\n");
#endif
	//if (fd) { fprintf(fd, "launching process\r\n"); fflush(fd);}
	SetHandleInformation( hChildStdoutWr, HANDLE_FLAG_INHERIT, 1);
	SetHandleInformation( hChildStdinRd, HANDLE_FLAG_INHERIT, 1);
	SetHandleInformation( hChildStdoutRd, HANDLE_FLAG_INHERIT, 0);
	SetHandleInformation( hChildStdinWr, HANDLE_FLAG_INHERIT, 0);

	// Get ready to start the Process
	sec.nLength = sizeof(sec);
	sec.lpSecurityDescriptor = NULL;
	sec.bInheritHandle = TRUE;

	GetStartupInfo(&si);
    si.dwFlags    = STARTF_USESTDHANDLES;

	si.hStdInput  = hChildStdinRd;
	si.hStdOutput = hChildStdoutWr;


    if (!CreateProcess(
                       NULL,
                       cmd,             // command line
                       NULL,            // process security
                       NULL,            // thread security
                       TRUE,            // inherit handles-yes
#ifdef DEBUG
                       0,
#else
                       CREATE_NO_WINDOW,               // creation flags
#endif
                       envp,            // environment block
                       NULL,            // current directory
                       &si,             // startup info
                       &pi))            // process info (out)
		{
			// The little thread closes this one.
//			if (errno == 2) continue;
			hChildStdinWr = 0;
			goto failed;
		}


	SAFE_FREE(cmd);
    // The Windows example closes these two HANDLES
    //CloseHandle(pi.hProcess);
	//WaitForInputIdle(pi.hProcess, 1000);

    CloseHandle(pi.hThread);
	CloseHandle(hChildStdinRd);
	hChildStdinRd = 0;
	CloseHandle(hChildStdoutWr);
	hChildStdoutWr = 0;

	// Close child's end of pipes (since we are parent).

	//if (fd) { fprintf(fd, "calling thread2\r\n"); fflush(fd);}
	// Start a new thread to handle stdin.
	// reads from newd->socket -> newd->file_socket.

	//if (fd) { fprintf(fd, "cleaning up\r\n"); fflush(fd);}

	parent_pipe->type = LION_TYPE_PIPE;

	// Assign pid.
	parent_pipe->process = pi.hProcess;
	parent_pipe->pid = pi.dwProcessId;
	parent_pipe->pipe_socket = hChildStdinWr;
	parent_pipe->pipe_socket2 = hChildStdoutRd; // So we can close it.
	parent_pipe->binary = newd->binary;
	parent_pipe->user_data = user_data;

	// Why does this code work? If we close it here? If we dont, we leak.
	//parent_pipe->file_socket = 0;
	//parent_pipe->file_socket2 = 0;
	//CloseHandle(hChildStdinWr);
	//CloseHandle(hChildStdoutRd);


	// We only want to use one bi-directional pipe in lion
	// so close the 2nd, and assign over the first. This is
	// ok because both threads are waiting on mutex. Once they
	// start they will both use ->socket, which is the same value.

	newd->socket = -1;
	newd->status = ST_DISCONNECT;  // will be released eventually.
	newd->pipe_socket  = 0;
	newd->pipe_socket2 = 0;

	if (lion_flags & LION_FLAG_TRACE) {
		lion_disable_trace(newd);
		lion_enable_trace(parent_pipe);
	}

#ifdef DEBUG
	printf("[parent] setting pending. %p\n", parent_pipe);
#endif


	// since we may or may not have data for select, force a loop to
	// send event.
	io_force_loop = 1;

	parent_pipe->status = ST_PENDING;


	return parent_pipe;







 failed:

	//if (fd) { fprintf(fd, "failed!\r\n"); fflush(fd);}

#ifdef DEBUG
	printf("[execv] failed %d/%d\n", errno, GetLastError());
#endif
	SAFE_FREE(cmd);
	// We failed. Set parent status to DISCONNECT, and release the child.
	// lnet will then issue _LOST to application



		//if (fd) { fprintf(fd, "freeing\r\n"); fflush(fd);}
#if 1
		if (child_pipe) {
			child_pipe->status = ST_DISCONNECT;
			lion_disconnect(child_pipe);
		}
		if (parent_pipe) {
			//parent_pipe->status = ST_PENDING;
			parent_pipe->status = ST_DISCONNECT;
			if (parent_pipe->socket > 0)
				sockets_close(parent_pipe->socket);
			parent_pipe->socket = -1;
		}
#endif

	if (hChildStdinRd) {
		CloseHandle(hChildStdinRd);
		hChildStdinRd = NULL;
	}

	// This one is actually closed by the child, if we have forked.
	if (hChildStdinWr) {
		CloseHandle(hChildStdinWr);
		hChildStdinWr = NULL;
	}
	// This one is actually closed by the child, if we have forked.
	if (hChildStdoutRd) {
		CloseHandle(hChildStdoutRd);
		hChildStdoutRd = NULL;
	}
	if (hChildStdoutWr) {
		CloseHandle(hChildStdoutWr);
		hChildStdoutWr = NULL;
	}


	newd->socket = -1;
	newd->status = ST_DISCONNECT;  // will be released eventually.

	// We want select to poll next iteration and enter processing
	// as we don't have a real fd to add to select, but we need to send
	// events from that part of the library
	io_force_loop = 1;
	//if (fd) { fprintf(fd, "returnring\r\n"); fflush(fd);}

	if (lion_flags & LION_FLAG_FULFILL) {
		newd->status = ST_PENDING;  // will be released eventually.
		newd->error_code = ENOENT;
		return newd;
	}

	return NULL;

}








//
// Connect two handles with pipes
//
int      pipe_make_pipes(connection_t *node1, connection_t *node2)
{
	int fd[2];
	// This is assuming you have socketpair function, and it has the UNIX
	// domain!

	// In future, we could also implement this by using our own functions
	// to open a listen on localhost, and connect to it, and tie the
	// two connections together.
	if( sockets_socketpair(fd) < 0 ) return -1;

	node1->socket = fd[0];
	node2->socket = fd[1];
	return 0; // Failure

}


int pipe_get_status( connection_t *node)
{
	int r;

#ifdef DEBUG
	printf("win32 get status: %p\n", node->thread);
#endif

	// Ok we don't know it yet.
	if (node->thread) {
		GetExitCodeThread( node->thread, &r );
		//printf("exit code %d\n", r);
		return r;
	}

	return -1;
}



void lion_want_returncode( connection_t *node)
{

	node->status = ST_WANTRETURN;

}



THREAD_SAFE static char *argv[MAX_ARG];

char **parser(char *s)
{
        char *ar, *arg;
        int argc;
        THREAD_SAFE static char name[1024];

        if (!s) return NULL;

        strncpy(name, s, sizeof(name));
        //name = strdup(s);
        ar   = name;


        // For each argument, build a list.
        for( argc = 0;
                 (arg = misc_digtoken(&ar, " \t\r\n")) && (argc < MAX_ARG-1);
                 argc++ ) {

#ifdef DEBUG
                printf("  '%s'\n", arg);
#endif
                argv[ argc ] = arg;

        }

        // null-terminate.
        argv[ argc ] = NULL;

#ifdef DEBUG
        printf("[parse] %d arguments\n", argc);
#endif

        return argv;

}



connection_t *lion_system(char *name_orig, int with_stderr,
                          int lion_flags, void *user_data)
{
	   connection_t *ret;
		char **argv;

        // Since the user might have passed us a static-string, which we can't
        // modify, we need to make a copy of it, and release.
        if (!name_orig) return NULL;


        argv = parser( name_orig );

		if (lion_flags & LION_FLAG_STDIO_AS_LION)
			ret = lion_execve2(argv[0], argv, NULL,
							with_stderr, lion_flags, user_data);
		else
			ret = lion_execve(argv[0], argv, NULL,
							with_stderr, lion_flags, user_data);
        return ret;

}










#endif

