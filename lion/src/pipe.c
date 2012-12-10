
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

// $Id: pipe.c,v 1.6 2010/10/26 06:38:41 lundman Exp $
// Pipe IO functions for the lnet library, fork, system.
// Jorgen Lundman January 9th, 2003.

#ifndef WIN32

#if HAVE_CONFIG_H
#include <config.h>
#endif

// Universal system headers
#define _BSD_SIGNALS

#if HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif

#if HAVE_UNISTD_H
# include <unistd.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#if __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif
#include <signal.h>
#include <string.h>

#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/stat.h>



#ifdef __sun__
extern const char *const sys_errlist[];
#endif



// Implementation specific headers

#include "connections.h"
#include "io.h"

#include "lion.h"

#include "sockets.h"

#include "pipe.h"

#include "misc.h"




static int last_signal_pid    = 0;
static int last_signal_status = 0;





__RCSID("$LiON: lundman/lion/src/pipe.c,v 1.6 2010/10/26 06:38:41 lundman Exp $");




//
// Perform a fork(). The child gets NULL returned to it.
// If there is a failure, a valid node is still issues, and
// the _LOST event is sent. If the child actually needs to
// get a handle as well, it can pass the "child" ptrptr.
//
connection_t *lion_fork(int (*start_address)(connection_t *, void *, void *),
						int flags, void *user_data, void *arg)
{
	connection_t *parent_pipe, *child_pipe;

	parent_pipe = connections_new();

	parent_pipe->type = LION_TYPE_PIPE;
	parent_pipe->user_data = user_data;


	if (flags & LION_FLAG_TRACE)
		lion_enable_trace(parent_pipe);

	if (parent_pipe->trace)
		fprintf(trace_file, "%p: lion_fork\n", parent_pipe);

	child_pipe = connections_new();

	child_pipe->type = LION_TYPE_PIPE;
	child_pipe->user_data = user_data; // child should call set_userdata.


	// Connect the two nodes.
	if (pipe_make_pipes(parent_pipe, child_pipe))
		goto failed;

#ifdef DEBUG
	printf("[pipe] parent %p/%d child %p/%d\n",
		   parent_pipe, parent_pipe->socket,
		   child_pipe, child_pipe->socket);
#endif


	// We have the pair of fd's connected.
	pipe_set_signals();



	parent_pipe->pid = fork();



	if ( parent_pipe->pid == -1 )
		goto failed;

	// Child work
	if (!parent_pipe->pid) {


		pipe_clear_signals();


		child_pipe->pid = getpid();

#ifdef DEBUG
		printf("[child] cleanup..\n");
#endif

		// Child returns NULL, so lets see if they passed a "child" ptr to
		// stuff the parent node into.

		// Clean up. We need to close everything, and free everything here
		// except for "child_pipe" and its fd.

		io_release_all( child_pipe );

		// Ready up the childs pipe.
		child_pipe->status = ST_PENDING;

		// since we may or may not have data for select, force a loop to
		// send event.
		io_force_loop = 1;

		_exit(start_address(child_pipe, child_pipe->user_data, arg));

		return NULL; // return to child.


	} /* NOT REACHED!! */


	// Parent!

	// We need to release the "child_pipe" on this side as we are parent.
	// and this node no longer exists on this end.

#ifdef DEBUG
	printf("[parent] releasing child node\n");
#endif

	child_pipe->status = ST_DISCONNECT;
	lion_disconnect(child_pipe);

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
		close(parent_pipe->socket);
	parent_pipe->socket = -1;   // This will notify it of failure.

	// We want select to poll next iteration and enter processing
	// as we don't have a real fd to add to select, but we need to send
	// events from that part of the library
	io_force_loop = 1;

	return parent_pipe;

}



struct lion_child_struct {
	char **argv;
	char **envp;
	int with_stderr;
};





int lion_child_starter(connection_t *child, void *arg1, void *arg2)
{
	int err;
	char **argv;
	char **envp;
	int with_stderr;
	struct lion_child_struct *passdata = (struct lion_child_struct *) arg2;

	/* child code! */
	if (!passdata) exit(-1);

	// Grab the data pass to us, and free the node.
	argv = passdata->argv;
	envp = passdata->envp;
	with_stderr = passdata->with_stderr;
	SAFE_FREE(passdata);


	// Change the socket from nonblocking to normal/blocking again
	fcntl(child->socket, F_SETFL,
		  (fcntl(child->socket, F_GETFL) & ~O_NONBLOCK));


	// Set stdin|out and possibly err to the pipe.

    dup2(child->socket, 0);
    dup2(child->socket, 1);

	if ( with_stderr ) {
		dup2(child->socket, 2);
	} else {
#if 0   // let stderr go to wherever it was going before.
		int i;
		i = open("/dev/null", O_WRONLY);
		dup2(i, 2);
		close(i);
#endif
	}
	close(child->socket);
	child->socket = 0;


	// Now attempt to start the new process
	//	printf("[fork] finally execv'ing: '%s'\n", argv[0]);

    // If given environment, use it, otherwise inherit.
	if (!envp)
		err = execv(argv[0], argv);
	else
		err = execve(argv[0], argv, envp);

	// Send the reason we failed. We are in nonblocking, so this
	// should only return once it has been sent.
	// This HAS to be the first thing we EVER print.
	lion_printf(child, "%s%d\n", PIPE_MAGIC, errno);



#ifdef DEBUG
	printf("[child_starter] execv failed! %d\n", err);
#endif

	// If we reach this place, execv failed.

	child->status = ST_DISCONNECT; // dont post events

	// Lets signal read() to return -1, not 0.
	//	lion_output( child, child, sizeof(child));
	//	shutdown( child->socket, SHUT_RDWR );

	lion_disconnect( child );       // release the only node left.

	// dont need to use childexit as this is the Unix only file
	// error code here isn't used.
	_exit(-1);

}


//
// Start a new process, leaving a pipe to the process.
// We fork, then exec a new process.
//
connection_t *lion_execve(char *base, char **argv, char **envp,
						  int with_stderr, int lion_flags, void *user_data)
{
	connection_t *parent = NULL; // the node that we give to parent!
	struct stat stbf;
	int err;
	struct lion_child_struct *passdata;

	// NULL means child.
#ifdef DEBUG
	{
		int i;
		printf("[lion_execv] executing base '%s'\n", base);
		for(i = 0; argv[i]; i++)
			printf("             '%s'\n", argv[i]);
	}
#endif

	// Before we fork() we might as well try to work out most failures
	// like no suchfile, or permission denied as they are the most common
	// problems. Saves us doing all that work.
	err = stat(argv[0], &stbf);

	// So, couldn't stat, or, it is a directory, or, it is not executable.
	if (err || (stbf.st_mode & S_IFDIR) ||
		!(stbf.st_mode & (S_IXUSR|S_IXGRP|S_IXOTH))) {

		// Return failure, if its fulfill allocate something
		if (!(lion_flags & LION_FLAG_FULFILL)) {
			return NULL;
		}

#ifdef DEBUG
		printf("[pipe] Early out failure: %d\n", errno);
#endif

		err = errno;

		parent = connections_new();
		parent->type = LION_TYPE_PIPE;
		parent->user_data = user_data;
		parent->error_code = (stbf.st_mode & S_IFDIR) ? EISDIR : EACCES;
		parent->socket = -1;
		parent->status = ST_PENDING;

		if (lion_flags & LION_FLAG_TRACE)
			lion_enable_trace(parent);

		return parent;

	}


	// Crap! We need to carry over "with_stderr" as well!
	passdata = (struct lion_child_struct *)
		malloc(sizeof(struct lion_child_struct));

	if (!passdata) {
		parent->socket = -1;
		return parent;
	}

	passdata->argv = argv;
	passdata->envp = envp;
	passdata->with_stderr = with_stderr;


	//	if ((parent = lion_fork( &child, lion_flags, user_data ))) {
	if ((parent = lion_fork( lion_child_starter, lion_flags,
							 (void *) user_data, (void *) passdata))) {

		// Parent code!
		parent->user_data = user_data;

		SAFE_FREE(passdata);

		// Lets have a sneak peak, did the fork work at all?
		if (parent->socket == -1) {
			// fork failed - so we just exit, the event gets posted
			// later.

			return parent;

		}

		SAFE_FREE(passdata);
		// fork worked (but the execv might still fail before parents
		// gets here! )
		parent->user_data = user_data;

		return parent;

	}

	// NOT REACHED.
	SAFE_FREE(passdata);

	return 0; // Stop warning.
}


static char *argv[MAX_ARG];

char **parser(char *s)
{
	char *ar, *arg;
	int argc;
	static char name[1024];

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
	char **argv;
	connection_t *ret;

	// Since the user might have passed us a static-string, which we can't
	// modify, we need to make a copy of it, and release.
	if (!name_orig) return NULL;


	argv = parser( name_orig );


	ret = lion_execve(argv[0], argv, NULL, with_stderr, lion_flags, user_data);


	return ret;

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









// *********************************************************************
//
// Signal handlers for children
//
// *********************************************************************



//
// If the child was lost, but the return code was not know, the
// application can call this function to request a further _CLOSED
// event to be issues once we know the return code.
//
void lion_want_returncode( connection_t *node)
{

	node->status = ST_WANTRETURN;

}



int pipe_get_status( connection_t *node)
{

	// Do we already know it?
	if (node->return_code != -1)
		return WEXITSTATUS(node->return_code);

	// Does it happen to be the last child that died?
	if (last_signal_pid == node->pid)
		return WEXITSTATUS(last_signal_status);

	// Ok we don't know it yet.
	return -1;


}



int pipe_signal_sub( connection_t *node, void *arg1, void *arg2)
{

	if (node->pid == (int) arg1)
		return 0;

	return 1;

}




//
// Be aware the we can get a sigchild for a child before the ->pid
// had been filled in. This is when fork lets the child run first, reached
// the execv() which fails. In this case, if we get a sigchild for a pid
// we can not match, we assign "last_signal_pid" and "last_signal_status".
// Then, when the pipe is closed, and we detect this and post the event to
// the application, we can match against these two variables and pass that
// along as well.
// In normal circumstances, we find the pid in connections pool, and assign
// the values there.
//
static RETSIGTYPE pipe_signal_child()
{
	int pid;
	int status;
	connection_t *node;

	// Some OS' need you to re-arm signals.
	pipe_set_signals();

#ifdef DEBUG
	printf("[pipe]: SIGCHLD received.\n");
#endif

	while ((pid = wait3(&status, WNOHANG, (struct rusage *) NULL)) > 0) {

		// we have pid and return-code (status) of a dead child.
		// look for a node with this pid, if one exists, we issue the
		// appropriate event for it. The event however, needs to be
		// sent at the correct time!
#ifdef DEBUG
		printf("[pipe] child %d code %d (%x)\n", pid, status, status);
#endif

		// Attempt to find it.
		node = connections_find(pipe_signal_sub, (void *) pid, NULL);

		if (node) {

			node->return_code = status;

		} else {

#ifdef DEBUG
			printf("[pipe] unknown pid (as of yet)\n");
#endif

			// Not found, assign variables
			last_signal_pid = pid;
			last_signal_status = status;

		}

	}

}




void pipe_set_signals(void)
{
#ifdef _POSIX_VERSION
  struct sigaction act;

  act.sa_handler = pipe_signal_child;
#ifdef SA_RESTART
  act.sa_flags = SA_NOCLDSTOP | SA_RESTART;
#else
  act.sa_flags = SA_NOCLDSTOP;
#endif
  sigemptyset(&act.sa_mask);
  sigaction(SIGCHLD, &act, NULL);

#else

  signal(SIGCHLD, pipe_signal_child);

#endif
}

void pipe_clear_signals(void)
{
#ifdef _POSIX_VERSION
  struct sigaction act;

  act.sa_handler = SIG_DFL;
  act.sa_flags = 0;
  sigemptyset(&act.sa_mask);
  sigaction(SIGCHLD, &act, NULL);
  sigaction(SIGPIPE, &act, NULL);

#else

  signal(SIGCHLD, SIG_DFL);
  signal(SIGPIPE, SIG_DFL);

#endif

}





#endif

