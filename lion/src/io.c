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

// $Id: io.c,v 1.35 2011/06/13 06:40:47 lundman Exp $
// Core input output functions, the main select() call and misc connectivity
// Jorgen Lundman November 5th, 1999


// Universal system headers

// To properly increase fd_set, we need to over-ride some things.
#ifdef FD_SETSIZE
#undef FD_SETSIZE
#endif

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#define FD_SETSIZE 4096
#define _DARWIN_UNLIMITED_SELECT

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <errno.h>
#include <string.h>

#include <signal.h>

// OS specific system headers
#ifdef WIN32
#define WINDOWS_LEAN_AND_MEAN
#include <winsock2.h>
#include <io.h>
#define HAVE_STDARG_H 1
#endif

#if HAVE_STDARG_H
#  include <stdarg.h>
#  define VA_START(a, f)        va_start(a, f)
#else
#  if HAVE_VARARGS_H
#    include <varargs.h>
#    define VA_START(a, f)      va_start(a)
#  endif
#endif
#ifndef VA_START
#  warning "no variadic api"
#endif


#include <fcntl.h>

#if HAVE_UNISTD_H
# include <unistd.h>
#endif

#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif



// Implementation specific headers

#include "connections.h"
#include "sockets.h"
#include "io.h"

#include "lion.h"
#include "pipe.h"
#include "lgroup.h"

#include "udp.h"

#ifdef WITH_SSL
#include "tls.h"
#include <openssl/bio.h>
#endif


//#define DEBUG

__RCSID("$LiON: lundman/lion/src/io.c,v 1.35 2011/06/13 06:40:47 lundman Exp $");




#ifndef MIN
#define MIN(a,b) (((a)<(b))?(a):(b))
#endif

#ifndef EWOULDBLOCK
#define EWOULDBLOCK EAGAIN
#endif


//#define NO_STARVATION_FIX
//#define FAVOUR_UPLOAD      // Used with starvation fix.

// Be aware that if you use SSL/TLS, and you disable the READITERATE feature
// you risk stalling connections. SSL reads in 16k chunks, but if buffersize
// in lion is smaller, we will get one chunk, then wait for more data (in
// select() which will never happen) so you will never get the last bit.
//#define NO_READITERATE
// This max should be defined to ensure buffersize * MAX > 16384.
// SSL/TLS uses 17k internally, we have to make sure we read at least that
// much. Which is 17408.
//#define READITERATE_MAX 17408
#define READITERATE_MAX 17408
//#define READITERATE_MAX 10240*1024
//#define READITERATE_MAX 32768



// The fd set holders for select.
THREAD_SAFE static fd_set io_fdset_read;
THREAD_SAFE static fd_set io_fdset_write;

THREAD_SAFE static int    io_num_fd_inuse = 0;

THREAD_SAFE static int    io_max_connections_allowed = 256;


// Do we compress packets larger than this, or disable?
THREAD_SAFE static int io_compress_level = 0;


// The time struct for select's timeout
THREAD_SAFE static struct timeval timeout;


// Current time, set once every loop, since time() is quite expensive
THREAD_SAFE time_t lion_global_time = 0;


// Force next loop to be poll (no timeout) and enter processing.
THREAD_SAFE int io_force_loop = 0;


THREAD_SAFE FILE *trace_file = NULL;





void lion_compress_level(int level)
{

	io_compress_level = level;

}



#ifdef WIN32
void io_test(SOCKET fd, int readwrite)
{
	fd_set gnoo;
	int result;
	timeout.tv_usec = 0;
	timeout.tv_sec  = 0;
	FD_ZERO(&gnoo);
	FD_SET(fd, &gnoo);
	result = select(1, readwrite?NULL:&gnoo,readwrite?&gnoo:NULL, NULL,
					&timeout);
	if (result < 0)
		printf("test of %d FAILED: %d %d\n", fd, result, WSAGetLastError());
	else
		printf("test of %d PASSED\n", fd);
}
#endif


//
// The core, main loop, select calls etc. This function only returns
// when the program should exit/abort.
//
// IN:  int utimeout, int timeout - both 0 == poll only, no block
// OUT: error, 0 for graceful exit.
//
int lion_poll(int utimeout, int stimeout)
{
	THREAD_SAFE static int dsize = -1;   // result from getdtablesize()
	int nret;    // return code from select()
	int was_forced = 0;

	//	printf ("\n> ");fflush(stdout);

	// Setup the timeval for timeout.
	// This is tuneable, but 30secs is pretty good for timeouts.
	timeout.tv_usec = utimeout;
	timeout.tv_sec  = stimeout;

	if (io_force_loop) {
		//		printf("f! ");
		timeout.tv_usec = 0;
		timeout.tv_sec  = 0;
		io_force_loop = 0;
		was_forced = 1;
	}

	// setup dsize, on Win32 it is ignored, on
	// unix it retrieved from getdtablesize(), or
	// OPEN_MAX, or use POSIX getrlimit(), or ....

	// Compute the maximum allowed connections we will allow.
	// Which is bigger? dsize or FD_MAXSET?
	if (dsize == -1)
		dsize = getdtablesize();

#ifdef NBBY
	io_max_connections_allowed = MIN(dsize, FD_SETSIZE * NBBY);
#else
	io_max_connections_allowed = MIN(dsize, FD_SETSIZE * 8);
#endif

	// Trim off, say, 10 connections
	io_max_connections_allowed -= 10;  // Fudge numbers are cool


	// Clear the fdset's
	FD_ZERO( &io_fdset_read  );
	FD_ZERO( &io_fdset_write );


	time(&lion_global_time);


	// Add all fd's that are required

	io_set_fdset( &io_fdset_read, &io_fdset_write );


	// There is a bug in WIN32 that happens if you call select()
	// with _no_ sockets at all (where it should sleep) it returns
	// an serious error instead.
	// If we are not to block, we can simply return here, otherwise
	// call sleep manually.
	//if (stimeout == 2) printf("cause\n");


	// Check if we should lower the timeout here due to timer-events.
	// If a timer is due inside the timeout specified, we need to change it
	// to a lower number.
	timers_select(&timeout);

#ifdef WIN32
	if (!io_num_fd_inuse) {

		timers_process();

		//if (was_forced) {
			io_process_fdset( &io_fdset_read, &io_fdset_write );
		//}

		// Not blocking? just return
		if (!utimeout && !stimeout) return 0;

		// Block for a bit
		Sleep(utimeout + (stimeout * 1000));
		return 1;

	}
#endif

	// Simply call select
	// Can optimise here by passing the highest fd+1 instead of dsize
	//
	//	printf ("s ");fflush(stdout);

	nret = select( dsize, &io_fdset_read, &io_fdset_write, NULL, &timeout);

	// Check any timer events before io? or after?
	timers_process();


	//	printf ("a ");fflush(stdout);

	switch ( nret ) {

	case -1:    // error
		// The only annoying thing here is SunOS/HPUX and some Ultrix
		// can (frequently) return EAGAIN so we ignore it, and restart,
		// all other errors mean exit.

		if (( errno == EAGAIN ) || ( errno == EINTR )) {
			return 0;
		}


		printf("************************************************\n");
		perror("select: ");

#ifdef WIN32
		printf("Dumping lion nodes: %d\n", WSAGetLastError());
		connections_dump();
#endif

#ifdef WIN32
		if (0){
			THREAD_SAFE static int run_once = 0;
			if (!run_once) {
				run_once = 1;
				printf("Windows hack. Forcing one round of processing...\n");
				io_process_fdset( &io_fdset_read, &io_fdset_write );
				Sleep(2000);
				return 0;
			}
		}

		// This code is naughty, as very Win32 specific. But only use to hunt
		// a bug.
		if (1) {
			int i;
		printf("Testing the fds in read fd_set, SNARGLEFOOPOINTOKUDU:\n");
		for (i = 0; i < io_fdset_read.fd_count; i++)
			io_test(io_fdset_read.fd_array[i], 0);
		printf("Testing the fds in write fd_set:\n");
		for (i = 0; i < io_fdset_write.fd_count; i++)
			io_test(io_fdset_write.fd_array[i], 1);
		}



#endif

		return -1;  // Signal we had a serious error.
		break;

	case 0:     // timeout - perform various timeout checks.
		//io_pushread();

		//		printf("T "); fflush(stdout);

		time(&lion_global_time);

		if (was_forced) {
			io_process_fdset( &io_fdset_read, &io_fdset_write );
		}


		// Check for rate_out caps
		io_process_rate();

		return 1;
		break;



	default:    // there was something happening on our sockets.
		time(&lion_global_time);

		io_process_fdset( &io_fdset_read, &io_fdset_write );


		// Right, to attempt to avoid starvation of clients, since we would
		// always parse the list in the same order, we "cycle" it here.
#ifndef NO_STARVATION_FIX

		connections_cycle();

#endif

		io_process_rate();

		return 0;

	} // switch

	return 0; // avoid the warning

}




// INTERNAL
// Add all fd's to the fdset as appropriate
// Encapsulated these calls so that winsock wont have to be included
// everywhere they are used.
//
void io_set_fdset( fd_set *fdset_read, fd_set *fdset_write )
{

	// These functions call io_set_fdsetread and io_set_fdsetwrite

	io_num_fd_inuse = 0;

	// Add all the sockets known to the system
	connections_find(io_set_fdset_sub, NULL, NULL);



}


// 0 means DON'T add to fd_set, and deny traffic
// none-zero means still within limit.
// This is called often, could make it faster, maybe #define.
int io_check_rate_in(connection_t *node)
{
	// We do this in order of efficiency
	// First check connection rate...
	if (node->rate_in) {

		// No time has passed, say ok
	    // time is assigned as now-1, this can't happen
		//		if (node->time_start >= lion_global_time)
		//			return 1;

		// No need for float maths, go for int approximation.
		if (( node->bytes_in / (lion_global_time - node->time_start ) / 1024)
			>= node->rate_in) {

			return 0;
		}
	}


	// Are we in a group?
	if (node->group && !lgroup_check_rate_in(node, node->group)) {

		//		node->group_bytes_in  = 0;
		return 0;
	}

	// Check global cap, but only on sockets? or on all? wtf do we do here.
	// Let's assume they mean just on network sockets.
	if (!lgroup_check_rate_in(node, LGROUP_GLOBAL)) {

		//node->group_bytes_in  = 0;
		return 0;
	}


	//node->group_bytes_in  = 0;

	return 1;

}



// 0 means post BUFFER_USED event, and request deny traffic
// none-zero means still within limit.
// This is called often, could make it faster, maybe #define.
int io_check_rate_out(connection_t *node)
{

	// Rate disabled (why are we here), say ok.
	if (node->rate_out) {

		if (( node->bytes_out / (lion_global_time - node->time_start ) / 1024)
			>= node->rate_out)
			return 0;

	}

	// Are we in a group?
	if (node->group && !lgroup_check_rate_out(node, node->group)) {

		//		node->group_bytes_out  = 0;
		return 0;
	}

	// Check global cap, but only on sockets? or on all? wtf do we do here.
	// Let's assume they mean just on network sockets.

	if (!lgroup_check_rate_out(node, LGROUP_GLOBAL)) {

		//node->group_bytes_out  = 0;
		return 0;
	}


	//node->group_bytes_out  = 0;

	return 1;

}


int io_set_fdset_sub( connection_t *node, void *arg1, void *arg2)
{

  if (node->trace)
    fprintf(trace_file, "%p: fd_set (%d) type %d r/w %d/%d status %d (want %d)\n",
	    node,
	    node->socket,
		node->type,
	    io_isset_fdsetread( node->socket ),
	    io_isset_fdsetwrite( node->socket ),
	    node->status,
	    node->want_mode);


  // Fast track to add descriptors based on SSL messages
  switch (node->want_mode) {
  case NET_NEG_WANT_READ:
  case NET_READ_WANT_READ:
  case NET_WRITE_WANT_READ:
    io_set_fdsetread ( node->socket );
    if (node->trace)
      fprintf(trace_file, "%p: want_mode %d set, adding to read.\n",
	      node, node->want_mode);
    break;
  case NET_NEG_WANT_WRITE:
  case NET_READ_WANT_WRITE:
  case NET_WRITE_WANT_WRITE:
    io_set_fdsetwrite( node->socket );
    if (node->trace)
      fprintf(trace_file, "%p: want_mode %d set, adding to write.\n",
	      node, node->want_mode);
    break;
  case NET_WANT_NONE:
  default:
    break; // Do nothing, carry on processing below.
  }



	// Perform the magic for sockets

	if (node->type == LION_TYPE_SOCKET) {

		switch ( node->status ) {

		case ST_PENDING:   // We are trying to connect(), add to both
		case ST_LISTEN:    // we've called accept(), add to both.
			if (node->socket >= 0) {

				// Allow them to "sleep" a listening socket.
				if (!node->disable_read) {

					// Do we seriously want to cap it during connection?
					//if (io_check_rate_in(node))
					io_set_fdsetread ( node->socket );

					io_set_fdsetwrite( node->socket );

				}

			} else {
				io_force_loop = 1;
			}
			break;


		case ST_CONNECTED: // Connect, look for new input
			if (!node->disable_read) {

				// Check CPS
				if (io_check_rate_in(node)) {
					io_set_fdsetread ( node->socket );

#if 0
					if (node->trace)
						fprintf(trace_file, "%p: adding to fd_set (%d)\n",
								node,
								node->socket);
#endif

				}
			}

			break;

		case ST_BUFFERFULL:// We have data in send buffer, w8 until we can send it
			if (!node->disable_read) {
				if (io_check_rate_in(node))
					io_set_fdsetread ( node->socket );
			}

			io_set_fdsetwrite( node->socket );
			break;

		case ST_READBUFFERFULL:// Full on reading, don't try to read.
			//io_set_fdsetread ( node->socket );
			//io_set_fdsetwrite( node->socket );
			break;

		case ST_NONE:      // Do nothing
		case ST_WANTRETURN:
		case ST_DISCONNECT:
		case ST_RESUMEREAD:
		default:
			break;

		}



		return 1; // Keep iterating

	} // SOCKET




	if (node->type == LION_TYPE_FILE) {

		switch ( node->status ) {

		case ST_PENDING:   // We are trying to connect(), add to both
		case ST_LISTEN:    // we've called accept(), add to both.
			if (node->socket >= 0) {
				if (io_check_rate_in(node))
					io_set_fdsetread ( node->socket );
			} else
				io_force_loop = 1;
			break;

		case ST_CONNECTED: // Connect, look for new input
			if (!node->disable_read) {
				if (io_check_rate_in(node))
					io_set_fdsetread ( node->socket );
			}
			break;

		case ST_BUFFERFULL:// We have data in send buffer, w8 until we can send it
			if (!node->disable_read) {
				if (io_check_rate_in(node))
					io_set_fdsetread ( node->socket );
			}

			io_set_fdsetwrite( node->socket );
			break;

		case ST_READBUFFERFULL:// Full on reading, don't try to read.
			//io_set_fdsetread ( node->socket );
			//io_set_fdsetwrite( node->socket );
			break;

		case ST_NONE:      // Do nothing
		case ST_WANTRETURN:
		case ST_DISCONNECT:
		case ST_RESUMEREAD:
		default:
			break;

		}


		return 1; // Keep iterating

	} // FILE


#ifdef WIN32

	if (node->type == LION_TYPE_PIPE_FILE) {

		switch ( node->status ) {

		case ST_PENDING:   // We are trying to connect(), add to both
		case ST_LISTEN:    // we've called accept(), add to both.
			io_force_loop = 1;
			break;

		case ST_CONNECTED: // Connect, look for new input
			if (!node->disable_read) {

				if (io_check_rate_in(node))
					io_set_fdsetread ( node->socket );

			}
			break;

		case ST_BUFFERFULL:// We have data in send buffer, w8 until we can send it
			if (!node->disable_read) {
				if (io_check_rate_in(node))
					io_set_fdsetread ( node->socket );
			}

			io_set_fdsetwrite( node->socket );
			break;

		case ST_READBUFFERFULL:// Full on reading, don't try to read.
			//io_set_fdsetread ( node->socket );
			//io_set_fdsetwrite( node->socket );
			break;

		case ST_WANTRETURN: // This node wants return code
			// If this node now has the returncode, force a loop so the
			// event is sent!
			if (node->return_code != -1)
				io_force_loop = 1;
			break;

		case ST_NONE:      // Do nothing
		case ST_DISCONNECT:
		case ST_RESUMEREAD:
		default:
			break;

		}


		return 1; // Keep iterating

	} // PIPE

#endif





	if (node->type == LION_TYPE_PIPE) {

		switch ( node->status ) {

		case ST_PENDING:   // We are trying to connect(), add to both
		case ST_LISTEN:    // we've called accept(), add to both.
			if (node->socket >= 0) {
				if (io_check_rate_in(node))
					io_set_fdsetread ( node->socket );
			} else
				io_force_loop = 1;
			break;

		case ST_CONNECTED: // Connect, look for new input
			if (!node->disable_read) {
				if (io_check_rate_in(node))
					io_set_fdsetread ( node->socket );
			}
			break;

		case ST_BUFFERFULL:// We have data in send buffer, w8 until we can send it
			if (!node->disable_read) {
				if (io_check_rate_in(node))
					io_set_fdsetread ( node->socket );
			}

			io_set_fdsetwrite( node->socket );
			break;

		case ST_READBUFFERFULL:// Full on reading, don't try to read.
			//io_set_fdsetread ( node->socket );
			//io_set_fdsetwrite( node->socket );
			break;

		case ST_WANTRETURN: // This node wants return code
			// If this node now has the returncode, force a loop so the
			// event is sent!
			if (node->return_code != -1)
				io_force_loop = 1;
			break;

		case ST_NONE:      // Do nothing
		case ST_DISCONNECT:
		case ST_RESUMEREAD:
		default:
			break;

		}


		return 1; // Keep iterating

	} // PIPE




	if (node->type == LION_TYPE_UDP) {

		switch ( node->status ) {

		case ST_PENDING:
			io_force_loop = 1;
			break;

		case ST_CONNECTED: // Connect, look for new input
			if (!node->disable_read) {

				// Check CPS
				if (io_check_rate_in(node))
					io_set_fdsetread ( node->socket );

			}
			break;

		case ST_BUFFERFULL:// We have data in send buffer, w8 until we can send it
			if (!node->disable_read) {
				if (io_check_rate_in(node))
					io_set_fdsetread ( node->socket );
			}
			io_set_fdsetwrite( node->socket );
			break;

		case ST_READBUFFERFULL:// Full on reading, don't try to read.
			//io_set_fdsetread ( node->socket );
			//io_set_fdsetwrite( node->socket );
			break;

		case ST_NONE:      // Do nothing
		case ST_WANTRETURN:
		case ST_DISCONNECT:
		case ST_RESUMEREAD:
		default:
			break;

		}


		return 1; // Keep iterating

	} // UDP






	return 1; // Keep iterating

}



//
// Set a fd into the fdset_read
//
void io_set_fdsetread( int fd )
{
    if (fd < 0) return;

	if (fd > io_num_fd_inuse)
		io_num_fd_inuse = fd;

	FD_SET( fd, &io_fdset_read );
}

//
// Set a fd into the fdset_read
//
void io_set_fdsetwrite( int fd )
{
    if (fd < 0) return;

	if (fd > io_num_fd_inuse)
		io_num_fd_inuse = fd;

	FD_SET( fd, &io_fdset_write );
}




//
// Clear a fd into the fdset_read
//
void io_clear_fdsetread( int fd )
{
	FD_CLR( fd, &io_fdset_read );
}

//
// Set a fd into the fdset_read
//
void io_clear_fdsetwrite( int fd )
{
	FD_CLR( fd, &io_fdset_write );
}







//
// Check all fdset's for new data
//
void io_process_fdset( fd_set *fdset_read, fd_set *fdset_write )
{


	connections_find(io_process_fdset_sub, NULL, NULL);

	// We also need to loop this again, and if there are any
	// node of type ST_DISCONNECT, we want to close them, and
	// abort the iteration immediately (as it may be in invalid state)
	// Since we change the linked list, we can only remove one node at a
	// time (sub returns 0). But since this is ONLY called when select()
	// says there was IO (not in TIMEOUT) we would only free ONE node here.
#if 1	// So, now we iterate until all NODES are released.

	while(connections_find(io_purge_sub, NULL, NULL)) {
#ifdef DEBUG
		printf("PURGE..");
#endif
	};
#endif

}

int io_purge_sub( connection_t *node, void *arg1, void *arg2)
{

	switch (node->status) {

	case ST_DISCONNECT:
#ifdef DEBUG
		printf("[io] disconnected %p - releasing...\n", node);
#endif
		lion_close(node);
		return 1;

	default:
		if (node->soft_closed) {
			lion_close(node);
			return 1;
		}
		if (node->want_mode == NET_WANT_CLOSE) {
			lion_disconnect( node );
			connections_free(node);
			return 0;
		}
	}

	if ((node->type == LION_TYPE_NONE)
#ifdef WIN32
		&& !node->start_address
#endif
		) {
#ifdef DEBUG
		printf("[io] Found node type none %p - releasing...\n", node);
#endif
		connections_free(node);
		return 0;
	}

	return 1;
}

int io_process_fdset_sub( connection_t *node, void *arg1, void *arg2)
{

#ifdef DEBUG
	//printf("check %d for want %d %p %d\n", node->socket, node->want_mode,
	//	   node, node->status);
#endif

  if (node->trace)
    fprintf(trace_file, "%p: process fd_set (%d) r/w %d/%d status %d (want %d)\n",
	    node,
	    node->socket,
	    io_isset_fdsetread( node->socket ),
	    io_isset_fdsetwrite( node->socket ),
	    node->status,
	    node->want_mode);


  // Fast track to add descriptors based on SSL messages
  switch (node->want_mode) {
#ifdef WITH_SSL
  case NET_NEG_WANT_READ:
    if (io_isset_fdsetread( node->socket )) {
		if ((node->auth_type == LION_SSL_SERVER))
			tls_cont_auth(node);
		else if ((node->auth_type == LION_SSL_CLIENT))
			tls_cont_clauth(node);
    }
    return 1;
  case NET_NEG_WANT_WRITE:
	  if (io_isset_fdsetwrite( node->socket )) {
		  if ((node->auth_type == LION_SSL_SERVER))
			  tls_cont_auth(node);
		  else if ((node->auth_type == LION_SSL_CLIENT))
			  tls_cont_clauth(node);
	  }
	  return 1;
	  break;
#endif
  case NET_READ_WANT_READ:
	  if (io_isset_fdsetread ( node->socket )) {
		  if (node->trace)
			  fprintf(trace_file, "%p: want_mode_read %d set, calling read\n",
					  node, node->want_mode);
		  io_input(node);
	  }
	  return 1;
	  break;

  case NET_READ_WANT_WRITE:
	  if (io_isset_fdsetwrite( node->socket )) {
		  if (node->trace)
			  fprintf(trace_file, "%p: want_mode_write %d set, calling read\n",
					  node, node->want_mode);
		  io_input(node);
	  }
	  return 1;
	  break;

  case NET_WRITE_WANT_READ:
	  if (io_isset_fdsetread ( node->socket )) {
		  if (node->trace)
			  fprintf(trace_file, "%p: want_mode_read %d set, calling write, buffer %d\n",
					  node, node->want_mode, node->outbuffer);
		  lion_output(node, NULL, 0);
	  }
	  return 1;
	  break;

  case NET_WRITE_WANT_WRITE:
	  if (io_isset_fdsetwrite( node->socket )) {
		  if (node->trace)
			  fprintf(trace_file, "%p: want_mode_write %d set, calling write, buffer %d\n",
					  node, node->want_mode, node->outbuffer);

		  lion_output(node, NULL, 0);

	  }
	  return 1;
	  break;

  case NET_WANT_NONE:
  default:
    break; // Do nothing, carry on processing below.
  }




	if (node->type == LION_TYPE_SOCKET) {


		switch ( node->status ) {

		case ST_LISTEN:    // we've called accept(), check both
			if (node->socket < 0) {  // failed
				_lion_userinput( node, node->user_data, LION_CONNECTION_LOST,
								 errno,
								 strerror(errno) ); // reason? FIXME

				node->status = ST_DISCONNECT;
				break;

			}

			if (io_isset_fdsetread( node->socket )) {

				// New connection, signal the user.
				_lion_userinput( node, node->user_data, LION_CONNECTION_NEW,
								 0, NULL );

				break;

			}



		case ST_PENDING:   // We are trying to connect(), check both

			if (node->socket < 0) {  // failed

				_lion_userinput( node, node->user_data, LION_CONNECTION_LOST,
								 errno,
								 strerror(errno) ); // reason? FIXME

				node->status = ST_DISCONNECT;
				break;

			}
			// Basically here, if we find we can write to this socket,
			// it means the connection established fine, and we can
			// move to ST_CONNECTED state.
			// If we get a read-trigger only, this means the connection failed


			// Check for failure first. If we can read(), but not write().
			if (io_isset_fdsetread( node->socket ) &&
				!io_isset_fdsetwrite( node->socket )) {

				// Blast, connection failed
				// there is a ceaveat here, the only way to get
				// errno to be set to the failure reason, is
				// to actually call read(), which we know will fail
				// but that sets the error code for us.

				sockets_read( node->socket, (unsigned char *)node->buffer, 1 );
				//perror("[Services]: services_process_fdset[ST_LISTEN]: connection failed");
				// Call the above layer
				_lion_userinput( node, node->user_data, LION_CONNECTION_LOST,
								 errno,
								 strerror(errno) ); // reason? FIXME

				node->status = ST_DISCONNECT;
				break;

			} // if fdread


			// Check for success, ie, we can write.
			if (io_isset_fdsetwrite( node->socket )) {

				// Sweet, connected.
				node->status = ST_CONNECTED;

				// Update time for rate
				node->time_start = lion_global_time - 1;

#ifdef WITH_SSL
				// We have to delay the connected event to user code
				// as they may start to send things until SSL is happy
				if (node->want_ssl == LION_SSL_CLIENT) {

					//				printf("connected and want_ssl is on, call auth\n");

					if (tls_clauth( node ) != 1) {  // Wank, failed.
						// Tricky, do I issue connected_even to client
						// followed by secure_fail, but they might send
						// information before the 2nd signal. I could just
						// close the socket and say it failed, but it is nice
						// to give the option to the client. SO, we
						// issue secure_failed first, even though, from the
						// client's POV it socket "doesn't exist" and they can
						// have the option to close it. If the socket
						// still is open we also issue connected_event.
						//					printf("posting failure1\n");

						_lion_userinput( node, node->user_data,
										 LION_CONNECTION_SECURE_FAILED,
										 0, NULL ); // FIXME, SSL_get_error

						// If they asked to disconnect, we just stop here.
						if (node->status == ST_DISCONNECT)
							break;


						// Falls down to send connected...


					} else {

						// Sweet, connected.
						// When connecting to localhost, the connection,
						// and challenge can be so fast, that its done in the
						// one call, and status is set to CONNECTED, only
						// to have us overwrite it here, and get stuck forever.
						// Bug fixed, 20050525
						//						node->status = ST_SSL_NEG;

						// Connection is in progress, so delay events
						break;

					}



				} else if (node->want_ssl == LION_SSL_SERVER) {


					// If we are in SERVER mode, we dont want to send the
					// connected event above, since we need to wait for
					// input, sense for SSL and so on.
					break;


				}




#endif


				_lion_userinput( node, node->user_data,
								 LION_CONNECTION_CONNECTED, 0, NULL );


				break;

			}

			break; // ST_PENDING & ST_LISTEN



		case ST_READBUFFERFULL:// We have data in send buffer, w8 until we can send it

			if (io_isset_fdsetwrite( node->socket )) {

				// ahh there is room to transmit again, purge the buffer
				lion_output( node, NULL, 0);

			}
			break;


		case ST_BUFFERFULL:// We have data in send buffer, w8 until we can send it

			if (io_isset_fdsetwrite( node->socket )) {

				//printf("res ");

				// ahh there is room to transmit again, purge the buffer
				lion_output( node, NULL, 0);

				// Detect sending failures.
				if (node->status == ST_DISCONNECT)
					break;

			}

			// Fall-through!!!
		case ST_CONNECTED: // Connect, look for new input


		  if (io_isset_fdsetread( node->socket )) {

		    //			printf("Read isset want %d\n", node->want_mode);


#ifdef WITH_SSL
		    // If we want SSL on incoming connections, lets
		    // auto-sense if this input is SSL or not. That way
		    // we don't swallow input incase it isnt.
		    if ( node->want_ssl == LION_SSL_SERVER ) {

		      tls_peek(node);
		      break;

		    }
#endif


		    // We have input, deal with it
		    io_input( node );



		  }


		  break; // ST_CONNECTED
		  break; // ST_BUFFERFULL



#ifdef WITH_SSL
		case ST_SSL_NEG:
			break;
#endif

		case ST_RESUMEREAD:
			io_input( node );
			break;


		case ST_DISCONNECT: // Wanting disconnect
		case ST_WANTRETURN:
		case ST_NONE:      // Do nothing
		default:

			break;

		}

		return 1; // Keep iterating

	} // SOCKET



	// ******************************************************************




	if (node->type == LION_TYPE_FILE) {


		switch ( node->status ) {


		case ST_LISTEN:    // there is no LISTEN state on files
			break;



		case ST_PENDING:   // file open, just say its good.

			if (node->socket < 0) {  // failed

				_lion_userinput( node, node->user_data, LION_FILE_FAILED,
								 errno,
								 strerror(errno) ); // reason? FIXME

				node->status = ST_DISCONNECT;
				break;

			}

			// We only get here if the file was opened successfully anyway
			// so we mark it good, and send the event. Note there is
			// a potential starvation here. Only opened a file, no sockets
			// have io and the file has nothing to read, the poll would
			// block until timeout.

			node->status = ST_CONNECTED;

			// Update time for rate
			node->time_start = lion_global_time - 1;

			_lion_userinput( node, node->user_data, LION_FILE_OPEN,
							 0, NULL );

			break; // ST_PENDING



		case ST_READBUFFERFULL:// We have data in send buffer, w8 until we can send it

			if (io_isset_fdsetwrite( node->socket )) {

				// ahh there is room to transmit again, purge the buffer
				lion_output( node, NULL, 0);

			}
			break;


		case ST_BUFFERFULL:// We have data in send buffer, w8 until we can send it
			if (io_isset_fdsetwrite( node->socket )) {

				// ahh there is room to transmit again, purge the buffer
				lion_output( node, NULL, 0);

			}

			// Fall-through!!!
		case ST_CONNECTED: // Connect, look for new input

			if (io_isset_fdsetread( node->socket )) {

				// We have input, deal with it
				io_input( node );

			}

			break; // ST_CONNECTED
			break; // ST_BUFFERFULL


		case ST_DISCONNECT: // Wanting disconnect
		case ST_WANTRETURN:
		case ST_NONE:      // Do nothing
		case ST_RESUMEREAD:
		default:

			break;

		}




		return 1; // Keep iterating
	} // FILE








#ifdef WIN32

	// ******************************************************************




	if (node->type == LION_TYPE_PIPE_FILE) {


		switch ( node->status ) {


		case ST_LISTEN:    // there is no LISTEN state on files
			break;



		case ST_PENDING:   // fork, fail, or success

			if (node->socket < 0) {  // failed

				_lion_userinput( node, node->user_data, LION_FILE_FAILED,
								 0, "file IO failedx" ); // reason? FIXME
				node->status = ST_DISCONNECT;
				break;

			}

			//printf("io - pipe_file for conncted\n");

			// Assume pipe was successful.
			node->status = ST_CONNECTED;

			// Send OPEN event.
			_lion_userinput( node, node->user_data, LION_FILE_OPEN,
							 0, NULL );

			// We assume the user does all of the wanted lseek() etc in the event above.
			// So we release the mutex so the child runs
			//ReleaseMutex( node->mutex );
			//node->mutex = NULL;
			if (node->event_start_in) {
#ifdef DEBUG
				printf("[io] Telling child %p to start_in\n", node);
#endif
				SetEvent(node->event_start_in);
			}
			if (node->event_start_out) {
#ifdef DEBUG
				printf("[io] Telling child %p to start_out\n", node);
#endif
				SetEvent(node->event_start_out);
			}




			// Was it already closed? If so, close it for reals.
			if (node->soft_closed) {

#ifdef DEBUG
				printf("[io] soft_closed set, closing.\n");
#endif
				// Close the file handle here, then the thread
				// will terminate like normal.
				lion_doclose(node, 2);
				//sockets_close(node->socket);
				//node->socket = -1;

				//_lion_userinput( node, node->user_data, LION_FILE_CLOSED,
				//				 0, NULL );
			}
			break; // ST_PENDING


		case ST_READBUFFERFULL:// We have data in send buffer, w8 until we can send it

			if (io_isset_fdsetwrite( node->socket )) {

				// ahh there is room to transmit again, purge the buffer
				lion_output( node, NULL, 0);

			}
			break;


		case ST_BUFFERFULL:// We have data in send buffer, w8 until we can send it
			if (io_isset_fdsetwrite( node->socket )) {

				// ahh there is room to transmit again, purge the buffer
				lion_output( node, NULL, 0);

			}

			// Fall-through!!!
		case ST_CONNECTED: // Connect, look for new input

			if (io_isset_fdsetread( node->socket )) {

				// We have input, deal with it
				io_input( node );

			}


			break; // ST_CONNECTED

		case ST_WANTRETURN:

			// Ok, if we have the return code now finally, issue the
			// event.

			if (node->return_code != -1) {

				_lion_userinput( node, node->user_data,
								 LION_PIPE_EXIT,
								 pipe_get_status(node),
								 NULL);

				// Note we set the status to disconnect here, AFTER the
				// event. Because the app can call to ask for another event
				// when the returncode is know, we over-rule this here. You
				// can only ask for it once, and it is known now anyway.
				node->status = ST_DISCONNECT;

			}


			break;


		case ST_DISCONNECT: // Wanting disconnect
		case ST_NONE:      // Do nothing
		case ST_RESUMEREAD:
		default:

			break;

		}




		return 1; // Keep iterating
	} // PIPE



#endif








	// ******************************************************************







	if (node->type == LION_TYPE_PIPE) {


		switch ( node->status ) {


		case ST_LISTEN:    // there is no LISTEN state on files
			break;



		case ST_PENDING:   // fork, fail, or success
			if (node->socket < 0) {  // failed

				if (node->error_code) {
					_lion_userinput( node, node->user_data, LION_PIPE_FAILED,
									 node->error_code,
									 strerror(node->error_code) );
				} else {

					// Who closes the socket here? XXX
					_lion_userinput( node, node->user_data, LION_PIPE_FAILED,
									 0, "fork failed" ); // reason? FIXME
				}

				node->status = ST_DISCONNECT;
				break;

			}

			// Assume pipe was successful.
			node->status = ST_CONNECTED;

			// Update time for rate
			node->time_start = lion_global_time - 1;

			_lion_userinput( node, node->user_data, LION_PIPE_RUNNING,
							 0, NULL );

#ifdef WIN32
			if (node->event_start_in) {
#ifdef DEBUG
				printf("[io] tellchild %p to start_in..\n", node);
#endif
				SetEvent(node->event_start_in);
			}
			if (node->event_start_out) {
#ifdef DEBUG
				printf("[io] tellchild %p to start_out..\n", node);
#endif
				SetEvent(node->event_start_out);
			}
#endif

			if (node->outbuffer > 0) {
				printf("[io] Connected, with data, setting READBUFFERFULL\n");
				node->status = ST_READBUFFERFULL;
			}

			break; // ST_PENDING



		case ST_READBUFFERFULL:// We have data in send buffer, w8 until we can send it

			if (io_isset_fdsetwrite( node->socket )) {

				// ahh there is room to transmit again, purge the buffer
				lion_output( node, NULL, 0);

			}
			break;


		case ST_BUFFERFULL:// We have data in send buffer, w8 until we can send it
			if (io_isset_fdsetwrite( node->socket )) {

				// ahh there is room to transmit again, purge the buffer
				lion_output( node, NULL, 0);

			}

			// Fall-through!!!
		case ST_CONNECTED: // Connect, look for new input

			if (io_isset_fdsetread( node->socket )) {

				// We have input, deal with it
				io_input( node );

			}

			break; // ST_CONNECTED

		case ST_WANTRETURN:

			// Ok, if we have the return code now finally, issue the
			// event.

			if (node->return_code != -1) {

				_lion_userinput( node, node->user_data,
								 LION_PIPE_EXIT,
								 pipe_get_status(node),
								 NULL);

				// Note we set the status to disconnect here, AFTER the
				// event. Because the app can call to ask for another event
				// when the returncode is known, we over-rule this here. You
				// can only ask for it once, and it is known now anyway.
				node->status = ST_DISCONNECT;

			}


			break;


		case ST_DISCONNECT: // Wanting disconnect
		case ST_RESUMEREAD:
		case ST_NONE:      // Do nothing
		default:

			break;

		}




		return 1; // Keep iterating
	} // PIPE








	// ******************************************************************







	if (node->type == LION_TYPE_UDP) {


		switch ( node->status ) {


		case ST_PENDING:   // We are trying to connect(), check both

			if (node->socket < 0) {  // failed

				_lion_userinput( node, node->user_data, LION_CONNECTION_LOST,
								 errno,
								 strerror(
										  node->error_code ?
										  node->error_code :
										  errno) );

				node->status = ST_DISCONNECT;
				break;

			}

			_lion_userinput( node, node->user_data, LION_CONNECTION_CONNECTED,
							 0,
							 NULL); // reason? FIXME

			node->status = ST_CONNECTED;
			break;


		case ST_READBUFFERFULL:// We have data in send buffer, w8 until we can send it

			if (io_isset_fdsetwrite( node->socket )) {

				// Need to clear UDP so other nodes steal it.
				io_clear_fdsetread( node->socket );

				// ahh there is room to transmit again, purge the buffer
				lion_output( node, NULL, 0);

			}
			break;


		case ST_BUFFERFULL:// We have data in send buffer, w8 until we can send it

			if (io_isset_fdsetwrite( node->socket )) {

				io_clear_fdsetwrite( node->socket );

				// ahh there is room to transmit again, purge the buffer
				lion_output( node, NULL, 0);

			}

			// Fall-through!!!
		case ST_CONNECTED: // Connect, look for new input

			if (io_isset_fdsetread( node->socket )) {

				io_clear_fdsetread( node->socket );
				//			printf("Read isset want %d\n", node->want_mode);

				// Special case, if WANT_READ was set, we just call...
				// We have input, deal with it
				io_input( node );

			}


			break; // ST_CONNECTED
			break; // ST_BUFFERFULL



		case ST_DISCONNECT: // Wanting disconnect
		case ST_WANTRETURN:
		case ST_RESUMEREAD:
		case ST_NONE:      // Do nothing
		default:

			break;

		}

		return 1; // Keep iterating

	} // UDP






	return 1; // Keep iterating
}



//
// Check if a fd is set in read
//
int io_isset_fdsetread( int fd )
{
	if (fd < 0) return 0;
	return FD_ISSET( fd, &io_fdset_read );
}

//
// Check if a fd is set in write
//
int io_isset_fdsetwrite( int fd )
{
	if (fd < 0) return 0;
	return FD_ISSET( fd, &io_fdset_write );
}




int io_near_full( void )
{

	if (io_num_fd_inuse >= io_max_connections_allowed)
		return 0;

	return 1;

}








int io_process_rate_sub( connection_t *node, void *arg1, void *arg2)
{

	if (!node->rate_out_capped)
		return 1;

	//printf("[io]: testing cap on %p\n", node);


	switch( node->status ) {

	case ST_PENDING:
	case ST_LISTEN:
	case ST_CONNECTED:
	case ST_BUFFERFULL:

		// If we have sent a previous event for rate_out reasons, and
		// we are no longer over the limit, send enabling event now
		if (io_check_rate_out(node)) {

			//			printf("[io] releasing out cap 2\n");
			_lion_userinput( node, node->user_data,
							 LION_BUFFER_EMPTY,
							 0, NULL);

			node->rate_out_capped = 0;
		}

		break;
	default:
		//printf("[io]: not %d\n", node->status);
		break;
	}

	return 1;
}



//
// If select() timedout, we need to also loop through and check for
// rate_out that should now be enabled again. If the loop is ran when
// select isn't idle, it is automatically cared for in process loop above.
// We could always call this loop, but then we loop twice everytime.
//
void io_process_rate( void )
{


	connections_find(io_process_rate_sub, NULL, NULL);


}









//
//
connection_t *lion_connect(char *host, int port, unsigned long iface,
						   int iport,
						   int lion_flags, void *user_data)
{
	connection_t *newd;
	unsigned long ip;

	newd = connections_new();

	newd->type = LION_TYPE_SOCKET;
	newd->user_data = user_data;

	if (lion_flags & LION_FLAG_TRACE)
		lion_enable_trace(newd);

	if (newd->trace)
		fprintf(trace_file, "%p: lion_connect\n", newd);

	// This function blocks.
	ip = sockets_gethost(host);


	if (!ip || ((newd->socket = sockets_connect(ip, port, iface, iport)) <0)) {

		// Signal above layer of failure
		if (lion_flags & LION_FLAG_FULFILL) {

			newd->status = ST_PENDING;
			// and sockets is == -1 it will be failed in pending.
			io_force_loop = 1;
			return newd;

		} else {

			_lion_userinput( newd, newd->user_data, LION_CONNECTION_LOST,
							 errno, strerror( errno ) );

			connections_free(newd);

			return NULL;

		}

	}

	// Let the network layer we are pending a connection
	newd->status = ST_PENDING;

	return newd;

}


connection_t *lion_listen(int *port, unsigned long iface, int lion_flags,
						  void *user_data)
{
	connection_t *newd;

	newd = connections_new();

	newd->type = LION_TYPE_SOCKET;

	newd->user_data = user_data;

	if (lion_flags & LION_FLAG_TRACE)
		lion_enable_trace(newd);

	if (newd->trace)
		fprintf(trace_file, "%p: lion_listen\n", newd);

	if (((newd->socket = sockets_make(port, iface,
									  (lion_flags & LION_FLAG_EXCLUSIVE)
									  )) < 0)) {

		// Signal above layer of failure
		if (lion_flags & LION_FLAG_FULFILL) {

			newd->status = ST_LISTEN;
			// and sockets is == -1 it will be failed in pending.
			io_force_loop = 1;
			return newd;

		}

		_lion_userinput( newd, newd->user_data, LION_CONNECTION_LOST,
						 errno, strerror( errno ) );

		connections_free(newd);

		return NULL;
	}

	// Let the network layer we are pending a connection
	newd->status = ST_LISTEN;

	return newd;

}


connection_t *lion_accept(connection_t *node, int close_old, int lion_flags,
						  void *user_data,unsigned long *remhost, int *remport)
{
	connection_t *newd;

	newd = connections_new();

	newd->type = LION_TYPE_SOCKET;

	newd->user_data = user_data;

	newd->socket = sockets_accept( node->socket, remhost, remport);


	if (close_old)
		node->status = ST_DISCONNECT;

	if (lion_flags & LION_FLAG_TRACE)
		lion_enable_trace(newd);

	if (newd->trace)
		fprintf(trace_file, "%p: lion_accept\n", newd);

	if (newd->socket < 0) {

		if (lion_flags & LION_FLAG_FULFILL) {

			newd->status = ST_PENDING;
			// and sockets is == -1 it will be failed in pending.
			io_force_loop = 1;
			return newd;
		}

		// Signal above layer of failure
		_lion_userinput( newd, newd->user_data, LION_CONNECTION_LOST,
						 errno, strerror( errno ) );

		connections_free(newd);

		return NULL;

	}


	newd->status = ST_PENDING;

	return newd;

}


#ifdef WIN32
int win32_bug_fix(connection_t *node, void *arg1, void *arg2)
{
    // Look for all node where arg->socket == node->socket
    if ((void *)node == arg1) return 1;
    if (!arg1) return 0;

    if (((connection_t *)arg1)->socket ==
        node->socket) {
        printf("[io] marked %p socket to -1\n", node);
        node->socket = -1;
    }

    return 1;
}
#endif





void lion_doclose(connection_t *node, int graceful)
{
	THREAD_SAFE static connection_t *rentry = NULL;

	if (!node)
		return;

	if (node->trace)
		fprintf(trace_file, "%p: lion_doclose(%d)\n", node, graceful);

	// If we are already in an event, delay this close.
	if (node->in_event) {
		if (graceful)
			node->soft_closed = 1;
		else
			node->want_mode = NET_WANT_CLOSE;
		if (node->trace)
			fprintf(trace_file, "%p: in_event, delaying close\n", node);
		return;
	}


	if (rentry == node) return;

	rentry = node;


	// graceful close?
	// Check if there is data to flush
	// if there is, signal we are technically closed, then in
	// the write section we actually close it, and send the event.


	if (graceful) {

#ifdef DEBUG
		printf("[io] graceful checks.\n");
#endif

#ifdef WIN32
		// In Windows, if we are writing a file, say in Write mode, and we've
		// not released the mutex then the data will never get written.
		// Soft_close (next poll loop releases mutex).
		//|| (node->mutex)

		// If we have mutex, check if the out ones have been triggered.
		if (node->event_end_out) {

			// It is possible, that with pipes to processes (who fail to start), that we
			// have not yet told the threads to start (so they can finish).
			if (node->event_start_out) {
#ifdef DEBUG
				printf("[io] telling event_out %p:%x to start\n", node, node->event_start_out);
#endif
				SetEvent(node->event_start_out);
			}


			// Close the socket to encourage it to finish.
			if (node->socket > 0) {
#ifdef DEBUG
				printf("[io] doclose(%p) closing socket %d\n", node,
					   node->socket);
#endif
				sockets_close(node->socket);
				node->socket = -1;
			}

			// Wait a bit for the thread to terminate.
#ifdef DEBUG
			printf("[io] doclose(%p) waiting for thread death %d\n", node, node->thread);
#endif

			if (WaitForSingleObject( node->event_end_out, 500 ) ==
				WAIT_TIMEOUT) {
					int r;
#ifdef DEBUG
				printf("[io] doclose(%p) thread is not exiting.\n", node);
#endif
				// If we have already been here once, we actively try to kill
				// the thread now.

#ifdef DEBUG
				printf("[io] doclose(%p) TerminateThread %d SKIPPING, not allowed\n", node, node->thread);
#endif

				GetExitCodeThread( node->thread, &r);

				if (r == STILL_ACTIVE) {
//					TerminateThread(node->thread, 1);

					GetExitCodeThread( node->thread, &r);
					if (r == STILL_ACTIVE) {
						node->soft_closed = 1;
#ifdef DEBUG
						printf("[io] timeout waiting for thread, trying again later: %d\n", r);
#endif
						rentry = NULL;
						return; // IDLE checker will re-try close.
					}
				}
			}

			// Thread has terminated.
#ifdef DEBUG
			printf("[io] releasing event_out %p\n", node);
#endif
			CloseHandle(node->event_start_out);
			CloseHandle(node->event_end_out);
			node->event_start_out = 0;
			node->event_end_out   = 0;
		} // Event "out" thread

		// We have an input handler
		if (node->event_end_in) {

			if (node->event_start_in) {
#ifdef DEBUG
				printf("[io] telling event_in %p:%x to start\n", node, node->event_start_in);
#endif
				SetEvent(node->event_start_in);
			}

			// Close the file_fd to encourage it to finish.
#if 0
			if (node->file_fd > 0) {
#ifdef DEBUG
				printf("[io] doclose(%p)_in closing file_fd %d\n", node,
					   node->file_fd);
#endif
				_close(node->file_fd);
				node->file_fd = -1;
			}
#endif

			if (node->pipe_socket > 0) {
#ifdef DEBUG
				printf("[io] doclose(%p)_in closing pipe_socket %d\n", node,
					   node->pipe_socket);
#endif
				CloseHandle(node->pipe_socket);
				node->pipe_socket = 0;
			}

			if (node->pipe_socket2 > 0) {
#ifdef DEBUG
				printf("[io] doclose(%p)_in closing pipe_socket2 %d\n", node,
					   node->pipe_socket2);
#endif
				CloseHandle(node->pipe_socket2);
				node->pipe_socket2 = 0;
			}

			// Wait a bit for the thread to terminate.
#ifdef DEBUG
			printf("[io] doclose(%p)_in waiting for thread death\n", node);
#endif

			if (WaitForSingleObject( node->event_end_in, 500 ) ==
				WAIT_TIMEOUT) {
#ifdef DEBUG
				printf("[io] doclose(%p)_in thread is not exiting.\n", node);
#endif
				// If we have already been here once, we actively try to kill
				// the thread now.
				if (node->soft_closed) {
					int r;

#ifdef DEBUG
					printf("[io] doclose(%p)_in TerminateThread SKIPPING, not allowed..\n", node);
#endif

					GetExitCodeThread( node->thread2
									   ? node->thread2 : node->thread, &r);

					if (r == STILL_ACTIVE) {
//						TerminateThread(node->thread2
//										? node->thread2 : node->thread, 1);
						GetExitCodeThread( node->thread2
										   ? node->thread2 : node->thread, &r);
						if (r == STILL_ACTIVE) {
#ifdef DEBUG
							printf("[io] timeout waiting for thread_in, trying again later\n");
#endif
							node->soft_closed = 1;
							rentry = NULL;
							return; // IDLE checker will re-try close.
						}
					}
				}
			}

			// Thread has terminated.
#ifdef DEBUG
			printf("[io] releasing event_in %p\n", node);
#endif
			CloseHandle(node->event_start_in);
			CloseHandle(node->event_end_in);
			node->event_start_in = 0;
			node->event_end_in  = 0;
		} // Event "out" thread

#endif // WIN32


		if ((node->status == ST_BUFFERFULL)) {

#ifdef DEBUG
			printf("lion_close(%p) with data, flushing...\n", node);
#endif

			if (node->status == ST_PENDING) {
				node->soft_closed = 1;
				rentry = NULL;
				return;
			}

			node->status = ST_BUFFERFULL;
			node->soft_closed = 1;

			if (node->trace)
				fprintf(trace_file, "%p: soft_close due to bufferdata\n", node);
			rentry = NULL;

			return;
		} // bufferful


#ifdef DEBUG
		printf("lion_close(%p) no data, closing.\n", node);
#endif

		node->soft_closed = 0;


	} // if (graceful)


	// ***************************************************************
	// If we get here, just close it.
#ifdef DEBUG
	printf("lion_doclose(%p) disconnect\n", node);
#endif



#ifdef WITH_SSL
	tls_close(node);
#endif

	node->want_mode = NET_WANT_NONE;


	switch(node->type) {

	case LION_TYPE_SOCKET:
#ifdef DEBUG
		//printf("[io] closing socket sock %d\n", node->socket);
#endif
		if (node->socket > 0)
			sockets_close( node->socket );
		node->socket = -1;
		if (node->status != ST_DISCONNECT)
			_lion_userinput( node, node->user_data,
							 LION_CONNECTION_CLOSED, 0, NULL);
		break;
	case LION_TYPE_FILE:
#ifdef WIN32
#ifdef DEBUG
		//printf("[io] closing socket file %d\n", node->socket);
#endif
		if (node->socket > 0)
			sockets_close( node->socket );
		node->socket = -1;
#if 1
		if (node->file_fd>0) {
			_close(node->file_fd);
#ifdef DEBUG
			//printf("[io] closing file_fd 1 %d\n", node->file_fd);
#endif
		}
		node->file_fd = -1;
#endif

		if (node->pipe_socket>0) {
			CloseHandle(node->pipe_socket);
			node->pipe_socket = 0;
		}
#else
		if (node->socket > 0)
			sockets_close( node->socket );
		node->socket = -1;
#endif
		if (node->status != ST_DISCONNECT)
			_lion_userinput( node, node->user_data,
							 LION_FILE_CLOSED, 0, NULL);
		break;
#ifdef WIN32
	case LION_TYPE_PIPE_FILE:
#ifdef DEBUG
		//printf("[io] closing socket pipefile %d\n", node->socket);
#endif
		if (node->socket > 0)
			sockets_close( node->socket );
		node->socket = -1;
		if (node->pipe_socket>0) {
			CloseHandle(node->pipe_socket);
#ifdef DEBUG
			printf("[io] pipe_file closing pipe_socket 2 %p:%d\n", node, node->pipe_socket);
#endif
		}
		node->pipe_socket = 0;
		if (node->pipe_socket2>0) {
			CloseHandle(node->pipe_socket2);
			node->pipe_socket2 = 0;
		}

		if (node->status != ST_DISCONNECT) {
			int r;
			//			printf("lion?disconnect\n");
			GetExitCodeThread( node->thread, &r);

#ifdef DEBUG
			printf("[io] GetExitCodeThread is %p:%d (%d == STILL_ACTIVE)\n", node,r, (r==STILL_ACTIVE));
#endif
			if (r == STILL_ACTIVE){
			//	TerminateThread(node->thread, 1);
			}

			if (node->thread2 && !r) {
				GetExitCodeThread( node->thread2, &r);
				if (r == STILL_ACTIVE) {
//					TerminateThread(node->thread2, 1);
				}
			}
#ifdef DEBUG
printf("Thread may potentially still be alive, zombying!\n");
#endif
			_lion_userinput( node, node->user_data,
							 LION_FILE_CLOSED, 0, NULL);

		}

		if (node->thread) {
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


		break;
#endif
	case LION_TYPE_PIPE:
#ifdef DEBUG
		//printf("[io] closing socket pipe %d\n", node->socket);
#endif

#ifdef WIN32
        if ((node->socket > 0) &&
            (node->thread) &&
            (node->thread2)) {
#ifdef DEBUG
            printf("[io] about to close %d, but there are 2 of me, so we will crash. This might be fixed now?\n", node->socket);
#endif
            // Attempt to find the sibling to this node
            connections_find( win32_bug_fix, (void *)node, NULL);

        }
#endif


		if (node->socket > 0)
			sockets_close( node->socket );
		node->socket = -1;
#ifdef WIN32
		if (node->pipe_socket>0) {
			CloseHandle(node->pipe_socket);
#ifdef DEBUG
			printf("[io] +closing pipe_socket %d\n", node->pipe_socket);
#endif
		}
		node->pipe_socket = 0;
		if (node->pipe_socket2>0) {
			CloseHandle(node->pipe_socket2);
#ifdef DEBUG
			printf("[io] +closing pipe_socket2 %d\n", node->pipe_socket2);
#endif
		}
		node->pipe_socket2 = 0;
#endif
		if (node->status != ST_DISCONNECT)
			_lion_userinput( node, node->user_data,
							 LION_PIPE_EXIT, 0, NULL);
		break;

	case LION_TYPE_UDP:
		// UDP we only close the actual UDP socket if we are the last one.
		if ((node->socket > 0) && !udp_find_others(node)) {
#ifdef DEBUG
			printf("[io] udp, no others %d actually closing.\n", node->socket);
#endif
			sockets_close( node->socket );
			node->socket = -1;
		}
		if (node->status != ST_DISCONNECT)
			_lion_userinput( node, node->user_data,
							 LION_CONNECTION_CLOSED, 0, NULL);
		break;
	default:
		break;
	}

	node->status = ST_NONE;
	node->type = LION_TYPE_NONE;

	rentry = NULL;

} // doclose




//
// close flushes any data in the output buffer before closing.
// the app will receive BUFFER_EMPTY followed by _CLOSED
//
void lion_close(connection_t *node)
{
	lion_doclose(node, 1);
}

// net_disconnect does NOT flush output buffer, simply drops everything
// and closes. Since we will generate an "event" to the user, and they may
// call lion_disconnect() again, we need to make sure we don't loop forever.
//
void lion_disconnect(connection_t *node)
{
	lion_doclose(node, 0);
}








#if 0
void lion_close(connection_t *node)
{
#ifdef WIN32
	int r = 0;
#endif
	THREAD_SAFE static connection_t *rentry = NULL;

	if (!node)
		return;

	if (node->trace)
		fprintf(trace_file, "%p: lion_close\n", node);

	// If we are already in an event, delay this close.
	if (node->in_event) {
		node->want_mode = NET_WANT_CLOSE;
		return;
	}


	if (rentry == node) return;

	rentry = node;


	// Check if there is data to flush
	// if there is, signal we are technically closed, then in
	// the write section we actually close it, and send the event.
#ifdef WIN32
	// In Windows, if we are writing a file, say in Write mode, and we've
	// not released the mutex then the data will never get written.
	// Soft_close (next poll loop releases mutex).
	//|| (node->mutex)

	if ((node->status == ST_BUFFERFULL)
		|| (node->event_start_out)) {

#else

		if ((node->status == ST_BUFFERFULL)) {

#endif // Win32


#ifdef DEBUG
			printf("net_close(%p) with data, flushing...\n", node);
#endif

		// In Windows, a file is a pipe, so it is in PENDING. The when it gets
		// connected, we release Mutex, and set to CONNECTED. If the caller has lion_open()
		// lion_printf(), lion_close(). It would never enter CONNECTED, and mutex never released.
		if (node->status == ST_PENDING) {
			node->soft_closed = 1;
			return;
		}

		node->status = ST_BUFFERFULL;
		node->soft_closed = 1;

		if (node->trace)
			fprintf(trace_file, "%p: soft_close due to bufferdata\n", node);

		return;
	}

#ifdef DEBUG
	printf("lion_close(%p) no data, closing.\n", node);
#endif

	node->soft_closed = 0;


#ifdef WITH_SSL
	tls_close(node);
#endif


	node->status = ST_DISCONNECT;
	node->want_mode = NET_WANT_NONE;

	switch(node->type) {

	case LION_TYPE_SOCKET:
		if (node->socket > 0)
			sockets_close( node->socket );
		node->socket = -1;
		_lion_userinput( node, node->user_data,
						 LION_CONNECTION_CLOSED, 0, NULL);
		break;
	case LION_TYPE_FILE:
		if (node->socket > 0)
			close( node->socket );
		node->socket = -1;
		_lion_userinput( node, node->user_data,
						 LION_FILE_CLOSED, 0, NULL);
		break;
#ifdef WIN32
	case LION_TYPE_PIPE_FILE:
		if (node->socket > 0)
			sockets_close( node->socket );
		node->socket = -1;

		//		printf("lion_close\n");

		GetExitCodeThread( node->thread, &r);

		if (node->thread2 && !r) {
			GetExitCodeThread( node->thread2, &r);
		}

		if (r) {
			_lion_userinput( node, node->user_data,
							 LION_FILE_FAILED, r, "IO error");
		} else {
			_lion_userinput( node, node->user_data,
							 LION_FILE_CLOSED, 0, NULL);
		}

		if (node->thread) {
			CloseHandle( node->thread );
			node->thread = NULL;
		}
		if (node->thread2) {
			CloseHandle( node->thread2 );
			node->thread2 = NULL;
		}


		break;
#endif
	case LION_TYPE_PIPE:
		if (node->socket > 0)
			sockets_close( node->socket );
		//			close( node->socket );
		node->socket = -1;
		_lion_userinput( node, node->user_data,
						 LION_PIPE_EXIT, 0, NULL);
		break;
	case LION_TYPE_UDP:
		// UDP we only close the actual UDP socket if we are the last one.
		if ((node->socket > 0) && !udp_find_others(node)) {
#ifdef DEBUG
			printf("[io] udp, no others %d actually closing.\n", node->socket);
#endif
			sockets_close( node->socket );
			node->socket = -1;
		}
		_lion_userinput( node, node->user_data,
						 LION_CONNECTION_CLOSED, 0, NULL);
		break;
	default:
		break;
	}

	node->type = LION_TYPE_NONE;
	rentry = NULL;

}


//
// net_disconnect does NOT flush output buffer, simply drops everything
// and closes. Since we will generate an "event" to the user, and they may
// call lion_disconnect() again, we need to make sure we don't loop forever.
//
void lion_disconnect(connection_t *node)
{
#ifdef WIN32
	int r = 0;
#endif
	THREAD_SAFE static connection_t *rentry = NULL;

	if (node->trace)
		fprintf(trace_file, "%p: lion_disconnect\n", node);

	// If we are already in an event, delay this close.
	if (node->in_event) {
		node->want_mode = NET_WANT_CLOSE;
		return;
	}

	if (rentry == node) return;

	rentry = node;

#ifdef WITH_SSL
	tls_close(node);
#endif

	node->want_mode = NET_WANT_NONE;


	switch(node->type) {

	case LION_TYPE_SOCKET:
		if (node->socket > 0)
			sockets_close( node->socket );
		node->socket = -1;
		if (node->status != ST_DISCONNECT)
			_lion_userinput( node, node->user_data,
							 LION_CONNECTION_CLOSED, 0, NULL);
		break;
	case LION_TYPE_FILE:
		if (node->socket > 0)
			close( node->socket );
		node->socket = -1;
		if (node->status != ST_DISCONNECT)
			_lion_userinput( node, node->user_data,
							 LION_FILE_CLOSED, 0, NULL);
		break;
#ifdef WIN32
	case LION_TYPE_PIPE_FILE:
		if (node->socket > 0)
			sockets_close( node->socket );
		node->socket = -1;
		if (node->status != ST_DISCONNECT) {

			//			printf("lion?disconnect\n");
			GetExitCodeThread( node->thread, &r);
			if (r == STILL_ACTIVE)
				TerminateThread(node->thread, 1);

			if (node->thread2 && !r) {
				GetExitCodeThread( node->thread2, &r);
				if (r == STILL_ACTIVE)
					TerminateThread(node->thread2, 1);

			}


			if (r) {
				_lion_userinput( node, node->user_data,
								 LION_FILE_FAILED, r, "file IO error");
			} else {
				_lion_userinput( node, node->user_data,
								 LION_FILE_CLOSED, 0, NULL);
			}
		}

		if (node->thread) {
			CloseHandle( node->thread );
			node->thread = NULL;
		}

		if (node->thread2) {
			CloseHandle( node->thread2 );
			node->thread2 = NULL;
		}


		break;
#endif
	case LION_TYPE_PIPE:
		if (node->socket > 0)
			sockets_close( node->socket );
		//			close( node->socket );
		node->socket = -1;
		if (node->status != ST_DISCONNECT)
			_lion_userinput( node, node->user_data,
							 LION_PIPE_EXIT, 0, NULL);
		break;
	case LION_TYPE_UDP:
		// UDP we only close the actual UDP socket if we are the last one.
		if ((node->socket > 0) && !udp_find_others(node)) {
#ifdef DEBUG
			printf("[io] udp, no others %d actually closing.\n", node->socket);
#endif
			sockets_close( node->socket );
			node->socket = -1;
		}
		if (node->status != ST_DISCONNECT)
			_lion_userinput( node, node->user_data,
							 LION_CONNECTION_CLOSED, 0, NULL);
		break;
	default:
		break;
	}


	node->status = ST_NONE;
	node->type = LION_TYPE_NONE;

	rentry = NULL;

}
#endif





int io_release_all_sub( connection_t *node, void *arg1, void *arg2)
{

#ifdef DEBUG_VERBOSE
	printf("io_release_all_sub(): %p %p\n", node, arg2);
#endif


	if (arg2 && (arg2 == (void *) node))  // is it the node to skip?
		return 1; // iterate to the next one.

	if (arg1) *((int *)arg1) = 1; // Found a node

	// We don't want to generate any events here, so set each node's status
	// to disconnect

	node->status = ST_DISCONNECT;

    // The thing here is, as a child, in unix, we can/should close all 'fds'
    // that we inherit, (but not on Windows), but, the ->ctx for TLS connections
    // point to the same ctx, and should not be released as child.
    // In Unix, we can simply overwrite ->ctx, to trigger a COW, and skip it.
    // On Windows, it is just one node/ptr, so we can not modify it.
#ifndef WIN32
#ifdef WITH_SSL
    if (arg2)
        node->ctx = NULL;
#endif

    // Close socket.
    lion_disconnect( node ); // refering to node after this call is illegal.

#endif

	connections_free( node );

	return 0;  // Since we delete this node, STOP iterating!

}



void io_release_all( connection_t *except_for )
{
	int not_empty;


	// iterate everything, passing a long an int so we know if we found
	// anything, so we can know when to stop. Also pass the except_for node
	// if any, to skip releasing a certain node.

	// Since we are removing nodes from the linked list we have to
	// stop iterating every time!
	do {
		not_empty = 0;
		connections_find(io_release_all_sub,
						 (void *)&not_empty, (void *)except_for );
	} while( not_empty );


}















#include <string.h>
#include "base64.h"
#include "zlib.h"

#define NET_COMPRESS_MAGIC "@CP!"   // 4 bytes only!


//
// Send a string to a service
//

int
#if HAVE_STDARG_H
lion_printf (connection_t *node, char const *format, ...)
#else
lion_printf (node, format, va_alist)
	 connection_t *node;
     const char *format;
     va_dcl
#endif
{
	va_list ap;
	THREAD_SAFE static char msg[STATIC_BUFFER_SIZE];
	int result;


	if (!node) return -1;


	VA_START (ap, format);

	result = vsnprintf(msg, sizeof(msg), format, ap);
	va_end(ap);

	return lion_send( node, msg, result);

}




//
// Wrapper for net_output, this check for the need to compress
//
int lion_send( connection_t *node, char *buffer, unsigned int len)
{
	THREAD_SAFE static char msg[STATIC_BUFFER_SIZE];
	unsigned int new_len;

	if (!node) return -1;

	// Do we compress it? Is it large enough?
	if (io_compress_level && (len >= io_compress_level)) {
		z_stream state;

#ifdef DEBUG
		printf("Output is %u bytes.\n", len);
#endif

		memset(&state, 0, sizeof(state));


		//state.strm.next_in = NULL;

		if (deflateInit2(&state, Z_BEST_COMPRESSION, 8,
						 -MAX_WBITS, 8, Z_DEFAULT_STRATEGY) == Z_OK) {
			// Z_BEST_COMPRESSION Z_DEFAULT_COMPRESSION

			// deflateReset(&state.strm);

			state.next_in  = (unsigned char *)buffer;
			state.avail_in = len;

			state.next_out = (unsigned char *)msg;
			state.avail_out = buffer_size;

			if (deflate(&state, Z_PACKET_FLUSH ) != Z_OK)
				printf("Deflate not OK\n");

#ifdef DEBUG
			printf("Compressed size: %u bytes.\n",
				   buffer_size - state.avail_out);
#endif

			deflateEnd(&state);


			new_len = buffer_size - state.avail_out;



			// Check if the final compressed size, after base64 is larger
			if (BASE64_ENCODE_SIZE(new_len) > len) // Compression is pointless
				return lion_output( node, buffer, len);


			// base64 the result
			// we assume it fits in original buffer! FIXME!!
			strncpy(buffer, NET_COMPRESS_MAGIC, buffer_size);
			base64_encode( (unsigned char *)msg,
						   new_len, (unsigned char *)&buffer[4] );
			strncat(buffer, "\r\n", buffer_size);
			new_len = strlen(buffer);


#ifdef DEBUG
			printf("Base64 size: %u\n", new_len);
#endif

			return lion_output( node, buffer, new_len);

		}


	}



	return lion_output( node, buffer, len);

}



int io_passbinary(connection_t *node)
{

	if ((node->status == ST_DISCONNECT) || (node->socket == -1)) {
#ifdef DEBUG
		printf("[io] passbinary with empty state: %p\n", node);
#endif
		return 0;
	}


	// Send this data to user.
	if (node->type == LION_TYPE_UDP) {

		udp_input(node, LION_BINARY, node->inbuffer, node->buffer);

	} else {

  		_lion_userinput(node, node->user_data, LION_BINARY,
						node->inbuffer, node->buffer);

	}

	node->inbuffer = 0;

	return 0;

}











int io_read(connection_t *node, char *buffer, int size)
{
	int result = 0;

	errno = 0;

	switch(node->type) {

	case LION_TYPE_SOCKET:
	case LION_TYPE_PIPE:    // getsocketpair
#ifdef WITH_SSL
		if (node->use_ssl) {
			result = tls_read(node, buffer,	size);
		} else {
#endif
			result = sockets_read(node->socket, (unsigned char *)buffer, size);
#ifdef WITH_SSL
		}
#endif
		break;



#ifdef WIN32
	case LION_TYPE_PIPE_FILE:
		result = sockets_read(node->socket, buffer, size);

#ifdef WITH_SSL
		// Decrypt file?
		if (node->use_ssl && (result > 0)) {

			BF_cfb64_encrypt(buffer,
							 buffer,
							 result,
							 &node->bf_key,
							 node->bf_ivec,
							 &node->bf_num,
							 BF_DECRYPT);

		}
#endif
		break;
#endif // Win32


	case LION_TYPE_FILE:
		result = read(node->socket, buffer, size);

		// Decrypt file?
#ifdef WITH_SSL
		if (node->use_ssl && (result > 0)) {

			BF_cfb64_encrypt((unsigned char *)buffer,
							 (unsigned char *)buffer,
							 result,
							 &node->bf_key,
							 node->bf_ivec,
							 &node->bf_num,
							 BF_DECRYPT);

		}
#endif
		break;

	case LION_TYPE_UDP:
		result = sockets_recvfrom(node->socket, (unsigned char *)buffer, size,
								  &node->udp_host, &node->udp_port);
	default:
		break;

	}


	// If we are a PIPE, and we have never read any input before
	// check if the input started with PIPE_MAGIC. If it does
	// fetch out the real error code so we can send FAILED to
	// the application.
	//	if (node->type == LION_TYPE_PIPE)
	//		printf("  >> %d %d\n", node->bytes_in, result);



	if ((node->type == LION_TYPE_PIPE) &&
#ifdef DEBUG    // in debug we let it print some stuff
		(node->bytes_in < 99) &&
#else
		(node->bytes_in == 0) &&
#endif
		(result > strlen(PIPE_MAGIC)) &&
		!memcmp(buffer, PIPE_MAGIC, strlen(PIPE_MAGIC))) {

		// We *know* that the int is followed by a non-digit (\n) so
		// we could just call atoi on it, but it could be a malicious
		// lion user could use that. So, lets force a null-term.
		buffer[result] = 0;

		node->error_code = atoi( &buffer[ strlen(PIPE_MAGIC) ] );
#ifdef DEBUG
		printf("[io_read] pipe errno hook: %d\n", node->error_code);
#endif

	}

	//printf("r%d %d %d     ", result, errno, WSAGetLastError()); fflush(stdout);


	return result;

}



int io_write(connection_t *node, char *buffer, int size)
{
	int sent = 0;

	errno = 0;

	switch(node->type) {

	case LION_TYPE_SOCKET:
	case LION_TYPE_PIPE:

#ifdef WITH_SSL
		if (node->use_ssl) {
			sent = tls_write( node, buffer, size);

			if (sent == -2) {

				sent = 0; // we need more? how do we signal this?

			}

		} else {

#endif
			sent = sockets_write( node->socket, (unsigned char *)buffer, size);

#ifdef WITH_SSL
		}
#endif
		break;


#ifdef WIN32
	case LION_TYPE_PIPE_FILE:

#ifdef WITH_SSL
		if (node->use_ssl && (size > 0)) {

			BF_cfb64_encrypt((unsigned char *)buffer,
							 (unsigned char *)buffer,
							 size,
							 &node->bf_key,
							 node->bf_ivec,
							 &node->bf_num,
							 BF_ENCRYPT);

		}
#endif

		sent = sockets_write( node->socket, buffer, size);
		break;
#endif // Win32



	case LION_TYPE_FILE:

		// Encrypt file?
#ifdef WITH_SSL
		if (node->use_ssl && (size > 0)) {

			BF_cfb64_encrypt((unsigned char *)buffer,
							 (unsigned char *)buffer,
							 size,
							 &node->bf_key,
							 node->bf_ivec,
							 &node->bf_num,
							 BF_ENCRYPT);

		}
#endif

		sent = write( node->socket, buffer, size);

#if 0 // simulate error
		if (node->bytes_out >10000) {
			sent = -1;
			errno = ENOSPC;
		}
#endif
		break;

	case LION_TYPE_UDP:
		// What do we do if host is NULL?
		sent = sockets_sendto( node->socket, (unsigned char *)buffer, size,
							   node->host, node->port);

	default:
		break;

	}

	if (sent > 0) {
		node->bytes_out += (bytes_t)sent;

		//		node->group_bytes_out += sent;
		lgroup_add_out(node, sent);
	}


	//	printf("w%d ", sent); fflush(stdout);
	if (node->trace)
		fprintf(trace_file, "%p: io_write(%d) => %d (0x%X): errno %d\n", node,
				node->outbuffer, sent, sent, errno);


	return sent;

}


//
// There was a sign that there is new information on a socket
// retrieve it and seperate it into string chunks to pass on.
//
int io_input(connection_t *node)
{
	int result = 0;
	char *line;   // Pointer to the current start of a string.
	int buffersize;
#ifndef NO_READITERATE
	int turns = 0;
#endif

	//	printf("Input on node %p type %d socket %d ssl %d\n",
	//    node, node->type, node->socket, node->use_ssl);


	// First, is this connection supposed to be binary?
	if (node->binary) {

#ifndef NO_READITERATE

		do {

			if (((turns++) * node->inbuffer_size) > READITERATE_MAX) {
#ifdef WITH_SSL
			// We can only break here if SSL_pending is 0, if
			// there is data in SSL buffer we HAVE to keep reading.
			if (node->use_ssl && !tls_pending(node))
#endif
				/* DANGLING-IF */
				break;
			}

			if (node->status == ST_DISCONNECT) break;
			if (node->want_mode == NET_WANT_CLOSE) break;

#endif
			errno = 0;

			// Set the block size "they" wish to deal with
			buffersize=node->inbuffer_size;
			if (node->inbinary_size)
				buffersize = node->inbinary_size;

			result = io_read(node,&node->buffer[node->inbuffer],
							 buffersize - node->inbuffer);

			if (result == -2)  //TLS needs more input
				return -2;

			if ((result == -1) &&
				((errno == EAGAIN) || (errno == EWOULDBLOCK)))
				return -2;

#ifdef WIN32
			if ((result == -1) && (WSAGetLastError() == WSAEWOULDBLOCK))
				return 0;
#endif

			//		printf(" io_input() %d -> %d\n", node->socket, result);


			if (result <= 0) {

				//printf("Closed but inbuffer is %d\n", node->inbuffer);
				// If we have left-over data in the buffer, we need to send it.

				node->status = ST_DISCONNECT;

				// If we are a pipe, get the real return code, if known.

				switch( node->type ) {

				case LION_TYPE_SOCKET:
				case LION_TYPE_UDP:
					_lion_userinput( node, node->user_data,
									 result < 0 ? LION_CONNECTION_LOST :
									 LION_CONNECTION_CLOSED,
									 errno, strerror( errno ) );
					return -1;
				case LION_TYPE_FILE:
#ifdef WIN32
				case LION_TYPE_PIPE_FILE:
#endif
					_lion_userinput( node, node->user_data,
									 result < 0 ? LION_FILE_FAILED :
									 LION_FILE_CLOSED,
									 errno, strerror( errno ) );

					// Special feature here, if we are reading a file, but
					// reach EOF, but expect it to grow (like tail -f) we
					// allow the application to disable read in the closed
					// event.
					if (node->disable_read) {
						node->status = ST_CONNECTED;
						return -2;
					}

					return -1;
				case LION_TYPE_PIPE:
#ifdef DEBUG
					printf("[1] ret %d\n", pipe_get_status(node));
#endif

					if (node->error_code) { // If we've set an error_code, it failed.

						_lion_userinput( node, node->user_data,
										 LION_PIPE_FAILED,
										 node->error_code,
										 strerror(node->error_code));

					} else {

						_lion_userinput( node, node->user_data,
										 LION_PIPE_EXIT,
										 pipe_get_status(node),
										 NULL);
					}

					return -1;
				default:
					return -1;

				}


			}

			node->inbuffer += result;

			node->bytes_in += (bytes_t) result;
			//node->group_bytes_in +=  result;


			// capping code to know about it.
			lgroup_add_in(node, result);


			// Pass it to program and all that logic
			io_passbinary(node);



#ifndef NO_READITERATE
			// Capping may have kicked in here, so it is no
			// good to keep spinning if so.
			// We can not cap here in SSL mode! Have to go MIN 16k
			if (!io_check_rate_in( node )) {
				return 0;
			}


		} while (!node->disable_read && (result > 0));
#endif


		return 0;


	} // END OF BINARY MODE




	// While we can get a line from the buffer parse it.
	// notice this can get seperated from here, so that we
	// simply call read() and fill the buffer here, and call
	// io_getline from elsewere. But not really required.

#ifndef NO_READITERATE

	do {

		if (((turns++) * node->inbuffer_size) > READITERATE_MAX) {
#ifdef WITH_SSL
			// We can only break here if SSL_pending is 0, if
			// there is data in SSL buffer we HAVE to keep reading.
			if (node->use_ssl && !tls_pending(node))
#endif
				/* DANGLING-IF */
				break;
		}

		if (node->status == ST_DISCONNECT) break;
		if (node->want_mode == NET_WANT_CLOSE) break;

#endif


		// Reset the current position counter here.
		line = NULL;

		while((result = io_getline(node, &line)) > 0) {

			if (node->trace) {
				int start, len;

				start = line - node->buffer;
				len = strlen(line);
				fprintf(trace_file, "%p: io_getline: turn %d: parsed 0x%X-0x%X of buffer 0x%X. With 0x%X remaining.\n",
						node,
#ifndef NO_READITERATE
						turns,
#else
						0,
#endif
						start,
						start + len,
						node->inbuffer,
						node->inbuffer - start - len
						);

				//	if ((node->inbuffer - start - len) == 0x0A)
				//	printf("break\n");

			} // if trace

			// Pass the string on to next layer
			//services_command(node, line);

			// Check if input is compressed
			if (!strncmp(NET_COMPRESS_MAGIC, line, 4)) {
				THREAD_SAFE static char decode[STATIC_BUFFER_SIZE];
				THREAD_SAFE static char large[STATIC_BUFFER_SIZE];
				int new_len;
				z_stream state;


#ifdef DEBUG
				printf("Compressed input '%s'\n", line);
#endif

				// We need to base64_decode it, then inflate it.
				new_len = base64_decode((unsigned char *)&line[4],
										(unsigned char *)decode,
										strlen( &line[4] ));

				if (new_len > 0) {

					// Call the inflator..

#ifdef DEBUG
					printf("Base64 length %d\n", new_len);
#endif
					memset(&state, 0, sizeof(state));


					if (inflateInit2(&state, -MAX_WBITS) == Z_OK) {

#ifdef DEBUG
						printf("inflate ready\n");
#endif

						//if (inflateInit2(&state, Z_BEST_COMPRESSION, 8,
						//		   -MAX_WBITS, 8, Z_DEFAULT_STRATEGY) == Z_OK) {
						// Z_BEST_COMPRESSION Z_DEFAULT_COMPRESSION

						// deflateReset(&state.strm);

						state.next_in  = (unsigned char *)decode;
						state.avail_in = new_len;

						state.next_out = (unsigned char *)large;
						state.avail_out = STATIC_BUFFER_SIZE;

						if (inflate(&state, Z_PACKET_FLUSH ) != Z_OK)
							printf("Inflate not OK\n");

#ifdef DEBUG
						printf("Compressed size: %u bytes.\n",
							   STATIC_BUFFER_SIZE - state.avail_out);
#endif

						inflateEnd(&state);

						if (node->type == LION_TYPE_UDP) {

							udp_input(node, LION_INPUT, strlen(large), large);

						} else {

							_lion_userinput( node, node->user_data,
											 LION_INPUT, strlen(large), large);

						}

						continue; // Don't call userinput below again.
					} // inflateInit

				} // new_len

			} // Compress magic

			if (node->type == LION_TYPE_UDP) {

				udp_input(node, LION_INPUT, strlen(line), line);

			} else {

				_lion_userinput( node, node->user_data, LION_INPUT,
								 strlen(line), line);
			}



			// User may change to binary mode here, deal with it.
			if (node->binary) {
				int start;

				start = line - node->buffer;
				start += strlen(line);

				if (node->trace) {
					fprintf(trace_file, "%p: io_getline: switched to binary, dumping remainder: %d.\n",
							node,
							node->inbuffer - start);
				}

				// We will skip up to 2 line feeds here.
				if (!node->buffer[start]) start++;
				if (!node->buffer[start]) start++;

				if (node->type == LION_TYPE_UDP) {

					udp_input(node, LION_BINARY,
							  node->inbuffer - start,
							  &node->buffer[start]);

				} else {

					_lion_userinput( node, node->user_data, LION_BINARY,
									 node->inbuffer - start,
									 &node->buffer[start]);
				}

				// Buffer is empty.
				node->inbuffer = 0;

				return result;
			} // if suddenly binary



		} // while getline


#ifndef NO_READITERATE

		if (node->trace) {
			fprintf(trace_file, "%p: io_getline: turn %d: result %d.\n",
					node,
					turns,
					result);
		}


		if (result == -2) return 0; // Failure? More input?
		if (result < 0) return result; // Failure? More input?

		// Capping may have kicked in here, so it is no
		// good to keep spinning if so.
		if (!io_check_rate_in( node )) {
			return 0;
		}


		if (node->want_mode != NET_WANT_NONE) break;


		// Keep calling read until we;
		// Hit READITERATE_MAX
		// Return -1 or -2 from io_getline
		// rate_in is triggered
		// state is DISCONNECT
		// or read is disabled
	} while (!node->disable_read);

#endif

	// If there is still crap in SSL, we could hang here forever.
#ifdef DEBUGX
	if (node && node->ctx && SSL_pending(node->ctx) > 0) {
		printf("%p: SSL_pending NOT ZERO; stalling possible.\n", node);
	}
#endif



	// We are done.
	// We return 0 when there are no more info to process
	// and -1 signifies the connection was lost.

	return result;
}



//
// Reads a buffer full, parses it until it finds a complete string (\r\n)
// then returns all full strings until there are no more. (Left overs are
// saved for the next buffer read)
//
int io_getline(connection_t *node, char **last)
{
	int len;                      // return int of read().
	int left;                     // remaining items
	THREAD_SAFE static char *next = NULL;            // strtok style PC holder.
	// While we are iterating getline to send lines of input to the user,
	// they might call disable_read, which we should honour.
	if (node->disable_read) {

		if (node->inbuffer && *last) {
			*last = next;
			left = (node->inbuffer - (*last - node->buffer));
			if (left > 0) {
				// Copy remaining.
				memcpy(node->buffer, *last, left);
				node->inbuffer = left;
				next = NULL;
			}
			return 0;
		}
		return 0;
	}


	if (node->status == ST_RESUMEREAD) {
		node->status = ST_CONNECTED;
		*last = node->buffer;
		next = *last;
	}


	// If last is NULL, it's the first call to this function for this
	// current iteration, so first we call read() to receive more data.
	if (!*last) {


		// Set it to start of the buffer
		*last = node->buffer;
		next = *last;


		// Retrieve more data from the socket.
		// We write it after any data that may already be left in the buffer
		// from a previous read. Generally doesn't happen, but we need to
		// allocate for it. (Buffer over/under -run hacking etc)

		//    if ((len = sockets_read(node->socket, &node->buffer[node->inbuffer],
		//                            BUFFER_SIZE - node->inbuffer)) <= 0) {

		// If we are using SSL and the socket is encrypted, call the appropriate
		// SSL functions to read DATA. This should probably be done in socket.c
		// but we dont pass the node, just the socket to that layer.
		len = io_read(node, &node->buffer[node->inbuffer],
					  node->inbuffer_size - node->inbuffer);

		if (node->trace)
			fprintf(trace_file, "%p: io_read(%d) => %d (0x%X) errno %d (want_mode %d)\n", node,
					node->inbuffer, len, len, errno,
				node->want_mode);


		if (len == -2)
			return -2;
		// Did the read request succeed?


		// Check if it was a fake error, so that we don't accidentally
		// close the connection.
		// Was this a real error or what?
		if ((errno == EWOULDBLOCK) ||
			(errno == EAGAIN)
#ifdef WIN32
			|| (WSAGetLastError() == WSAEWOULDBLOCK)
#endif
			)
			return -2; // Need more data.




		if (len <= 0) {

			node->status = ST_DISCONNECT;

			if (node->inbuffer) {
				if (node->type == LION_TYPE_UDP) {
					udp_input(node, LION_INPUT, node->inbuffer, node->buffer);
				} else {
					_lion_userinput( node, node->user_data,
									 LION_INPUT, node->inbuffer, node->buffer);
				}
				node->inbuffer = 0;
			}



			switch( node->type ) {

			case LION_TYPE_SOCKET:
			case LION_TYPE_UDP:
				_lion_userinput( node, node->user_data,
								 len < 0 ? LION_CONNECTION_LOST :
								 LION_CONNECTION_CLOSED,
								 errno, strerror( errno ) );
				return -1;

			case LION_TYPE_FILE:
#ifdef WIN32
			case LION_TYPE_PIPE_FILE:
#endif
				_lion_userinput( node, node->user_data,
								 len < 0 ? LION_FILE_FAILED :
								 LION_FILE_CLOSED,
								 errno, strerror( errno ) );
				return -1;

			case LION_TYPE_PIPE:
#ifdef DEBUG
				//printf("[io] 2 read %d\n", len);
#endif

				if (node->error_code) {  // If we've set an error_code, it failed.

					_lion_userinput( node, node->user_data,
									 LION_PIPE_FAILED,
									 node->error_code,
									 strerror(node->error_code));

				} else {

					_lion_userinput( node, node->user_data,
									 LION_PIPE_EXIT,
									 pipe_get_status(node),
									 NULL);
				}

				return -1;

			default:
				return -1;

			}



		}


		// Move along the PC according to how much was read.
		node->inbuffer += len;
		node->bytes_in += (bytes_t) len;
		//		node->group_bytes_in += len;
		lgroup_add_in(node, len);


	} else {  // last == NULL

		// last isn't NULL, so we are now still parsing buffer content, if any

		*last = next;

	}


	// Right we now have some data in the buffer, extract a string.

	do {


		if ((int)(next - node->buffer) >= node->inbuffer) {


			// Compute what is remaining
			left = (node->inbuffer - (*last - node->buffer));


			// we only bother keeping the remaining if it is more than
			// 3, since 2 is just \r\n.

			if (left > 0) {

				// Copy remaining.
				memcpy(node->buffer, *last, left);

				// Reset pointers and PC
				node->inbuffer = left;
				next = NULL;

				if (node->trace)
					fprintf(trace_file, "%p: leaving incomplete input of 0x%X bytes until next read.\n",
							node,
							left);


                if (node->inbuffer >= node->inbuffer_size) {

                    if (node->trace)
                        fprintf(trace_file, "%p: full buffer without a newline, returning entire buffer\r\n",
                                node);
                    // But we do need to terminate the string. This is why we allocate
                    // +1 byte on buffers.
                    node->buffer[ node->inbuffer_size-1 ] = 0;
                    node->inbuffer = 0;
                    next = NULL;
                    return 0;
                }


			} // some left

			// stop processing
			return 0;

		}




		// Check if we've hit the end.
		if ((*next == '\n') || (*next == '\r')) { /* Got a line */

			// yep, skip past all blank \r or \n.

			// ass, this wipes out empty lines too.. we will only zap
			// the next char, if we found a \r and the next is a \n
			// bounds check, '\r' could be last char, which means +1
			// is beyond buffer.

			if ((*next == '\r') &&
				(next < &node->buffer[ node->inbuffer_size - 1 ]) &&
				(next[1] == '\n'))
				*(next++) = (char) 0;

			*(next++) = (char) 0;

			//while((*next == '\n') || (*next == '\r'))
			// *(next++) = (char) 0;

			// and replace them with \0. (Terminate string).

			// If we've completely finished parsing the input
			// start again from the start. If not, "next" is ready for
			// next time around.
			if (node->inbuffer - ((int)(next - node->buffer)) <= 0) {
				node->inbuffer = 0;
			}

			// Signal we have a valid string.
			return 1;
		}


        // If we have a whole buffer full, but not a single newline, we need to
        // abort looking for newlines, and just return the full buffer. It is
        // up to the developer to increase buffer_size, or handle fragmented
        // strings.
		if ((next > (node->buffer + node->inbuffer_size)) /*||
            (node->inbuffer >= node->inbuffer_size)*/) {

            if (node->trace)
                fprintf(trace_file, "%p: full buffer without a newline, returning entire buffer\r\n",
                        node);
            // But we do need to terminate the string. This is why we allocate
            // +1 byte on buffers.
            node->buffer[ node->inbuffer_size-1 ] = 0;
			node->inbuffer = 0;
            next = NULL;
			return 0;
		}

		// Advance to next char
		next++;

	} while(1);

	// NOT REACHED
	return 0;  // avoid the warning
}




//
// Send some information. If we can't send, or only part, store
// it in the output buffer, and set our status to ST_BUFFERFULL.
//
// FIXME: Currently the logic is if you try to send more on a
// handle in state ST_BUFFERFULL, it will attempt to call io_write again
// this is uneccessary! We should only buffer in this situation.
//
int lion_output( connection_t *node, char *buffer, unsigned int len)
{
	int sent = 0;
	int send_buffer_empty = 0; // Set this if we should send event.
	unsigned long suggested_size = 0;
	char *tmp_ptr = NULL;


	if (!node) return -1;

	// if the API calls lion_open(), wait for FILE_OPEN event, then proceed to call
	// lion_printf() functions (and possibly even lion_close()), we need to actually release
	// the IO threads here.
#ifdef WIN32
	if (node->event_start_in) {
#ifdef DEBUG
		printf("[io] lion_output: Telling child %p to start_in\n", node);
#endif
		SetEvent(node->event_start_in);
		CloseHandle(node->event_start_in);
		node->event_start_in = 0;
	}
	if (node->event_start_out) {
#ifdef DEBUG
		printf("[io] lion_output: Telling child %p to start_out\n", node);
#endif
		SetEvent(node->event_start_out);
		CloseHandle(node->event_start_out);
		node->event_start_out = 0;
	}
#endif


	// assert's
	if ( node->status == ST_DISCONNECT) return -1;

	// First we check if there is anything already in the buffer, if
	// so, we attempt to send that (first?)

	if ( node->outbuffer ) {



		// Ensure we don't try to write packets larger than THRESHHOLD
		// or they will be dropped.
		// Check for SSL of course
		sent = io_write( node, node->obuffer, node->outbuffer);

		//		printf("io_write() tried to send %d but managed %d\n", node->outbuffer);

		// did we manage to send it all?
		if (sent == node->outbuffer) {

			if (node->status == ST_BUFFERFULL) {

				node->status = ST_CONNECTED;
			}

			node->outbuffer = 0;


			// Check if we were soft_closed already, and the buffer is
			// now empty, we can close it for real.
			if (node->soft_closed) {

				node->status = ST_DISCONNECT;

				switch (node->type ) {

				case LION_TYPE_UDP:
				case LION_TYPE_SOCKET:
					_lion_userinput( node, node->user_data,
									 LION_CONNECTION_CLOSED, 0, NULL);
					return 0;
				case LION_TYPE_FILE:
#ifdef WIN32
				case LION_TYPE_PIPE_FILE:
#endif
					_lion_userinput( node, node->user_data,
									 LION_FILE_CLOSED, 0, NULL);
					return 0;
				case LION_TYPE_PIPE:
					_lion_userinput( node, node->user_data,
									 LION_PIPE_EXIT, 0, NULL);
					return 0;
				default:
					return 0;
				}

			}




			// Technically we could send buffer empty event here, but
			// it is possible that the data passed to us fill it again
			// so we delay it, until we are sure.
			send_buffer_empty = 1;


		} else { // rats, we still have data to buffer.

			// Will the buffer be too full?  If so what choice do we have but to
			// either discard the info, or drop the connection with buffer overrun?


			if (sent < 0) {

				// Was this a real error or what?
				if ((errno == EWOULDBLOCK) ||
					(errno == EAGAIN)
#ifdef WIN32
					|| (WSAGetLastError() == WSAEWOULDBLOCK)
#endif
					) {

					sent = 0;

				} else {

#ifdef DEBUG
					printf("net_output(%p:%d:%d) Failed! Tried to send %d bytes\n",
						   node, node->socket,
#ifdef WIN32
						WSAGetLastError(),
#else
						   errno,

#endif
						   node->outbuffer);

#endif

					switch ( node->type ) {

					case LION_TYPE_SOCKET:
					case LION_TYPE_UDP:
						_lion_userinput( node, node->user_data,
										 len < 0 ? LION_CONNECTION_LOST :
										 LION_CONNECTION_CLOSED,
										 errno, strerror( errno ) );
						break;
					case LION_TYPE_FILE:
#ifdef WIN32
					case LION_TYPE_PIPE_FILE:
#endif
						_lion_userinput( node, node->user_data,
										 len < 0 ? LION_FILE_FAILED :
										 LION_FILE_CLOSED,
										 errno, strerror( errno ) );
						break;
					case LION_TYPE_PIPE:
						_lion_userinput( node, node->user_data,
										 len < 0 ? LION_PIPE_FAILED :
										 LION_PIPE_EXIT,
										 errno, strerror( errno ) );
						break;
					}

					node->status = ST_DISCONNECT;
					return -1;

				}

			} // if (sent < 0)




			//
			// BUG 2004/02/13
			//
			// IF   we have newdata,
			// AND  we had data in buffer,
			// AND  we had a partial write on buffer data,
			// THEN we need to keep remaining, PLUS newdata and
			// return.

			// So, add on the space needed with regards to "len" the newdata.
			while (node->outbuffer + (node->outbuffer - sent) + len >
				   node->outbuffer_size) {


				// This assumes that "outbuffer_size * 2" is always >
				// than "len" can ever be. "len" SHOULD always be less
				// than the original buffer size. But we could change this in
				// future...


				// Grow buffer
				//node->obuffer =
				tmp_ptr =
					realloc(node->obuffer, node->outbuffer_size * 2);

				// BUG 2004/01/22 - We over-write the original pointer, so
				// even though we disconnect the user, we never free() the
				// memory (yes, realloc fails, but it doesn't release the
				// old buffer!).
				// Fixed.

				if (!tmp_ptr) {

					//					log_printf(LOG_DEBUG, "%08x - Attempted to grow output buffer 1 but failed!\n",
					//	node->socket);

#ifdef DEBUG
					printf("%p:dumping connection due to realloc failure.\n",
						   node);
#endif

					node->outbuffer = 0;
					node->outbuffer_size = 0;
					node->status = ST_DISCONNECT;

					return -1;

				}


				node->obuffer = tmp_ptr;

				// We grew in size
				node->outbuffer_size *= 2;

#ifdef DEBUG
				printf("%08x: Grew output buffer 1 to %d\n",
					   node->socket, node->outbuffer_size);
#endif

			} // while

			// If something was sent, but not all, we need to store it.
			// copy the partial write.
			if (sent > 0) {

				memcpy( node->obuffer,
						&node->obuffer[ sent ], node->outbuffer - sent );

				node->outbuffer -= sent;

			}

			// Copy the new data
			if (len) {

				// BUG 2005/01/19. We copied new data to the START of
				// output buff, when there might be data in there already
				//
				memcpy( &node->obuffer[node->outbuffer],
						buffer, len );

				node->outbuffer += len;

#ifdef DEBUG
				//printf("[io] partial buffer write with new data? resending event\n");
#endif

				_lion_userinput( node, node->user_data,
								 LION_BUFFER_USED,
								 0, NULL);
			}


			if (node->trace) {
			  fprintf(trace_file, "%p: setting BUFFERFULL\n",
				  node);
			}

			// ZZZ!!!!
			node->status = ST_BUFFERFULL;

			// We can NOT let it fall down here to try to send data, since
			// then new data goes before buffered.
			// technically, we should not be here if the app obeys the
			// buffering events. Send another?

			return sent;

		} // else (didnt send all)

	}  // if node->outbuffer




	// Check if we were passed new information to send
	if (!buffer || ( len == 0 )) {

		// Check for capping..
		if (!io_check_rate_out( node )) {

			// Ok, this needs to be capped. So we send the "fake" event.
			if (!node->rate_out_capped) {
				//printf("[io] sending out cap\n");
				_lion_userinput( node, node->user_data,
								 LION_BUFFER_USED,
								 0, NULL);
			}

			node->rate_out_capped = 1;
			send_buffer_empty = 0;

		}



		// Send buffer empty?
		if (send_buffer_empty) {

			if (node->status == ST_BUFFERFULL) {
				node->status = ST_CONNECTED;
			}

			_lion_userinput( node, node->user_data,
							 LION_BUFFER_EMPTY,
							 0, NULL);

		}

		return sent;
	}


	// Attempt to send the new info, if we've already failed above
	// we don't attempt it again, just incase it would pass thru (and then
	// we'd get data out of order)

	sent = 0;
	errno = 0;


	// If we try to send more than THRESHHOLD we will just trigger our own
	// code and drop it. So we send maximum upto THRESHHOLD


	sent = io_write( node, buffer, len);

	//	printf("io_write(%p) send %d => %d '%s'\n", node,
	//   len, sent, "data" /*buffer*/);



#ifdef WIN32
	if (WSAGetLastError() == WSAEWOULDBLOCK)
		sent = 0;
#endif


	if ((sent < 0) && ((errno == EWOULDBLOCK) ||
					   (errno == EAGAIN))) {
		sent = 0;

		//		fcntl(node->socket, F_SETFL, (fcntl(node->socket, F_GETFL) & ~O_NONBLOCK));


	}



	if (sent < 0) {
		// Was this a real error or what?

		switch ( node->type ) {

		case LION_TYPE_UDP:

			// These are not permanent failures

#ifdef WIN32
			switch(WSAGetLastError()) {
			case WSAEINPROGRESS:
			case WSAENOBUFS:
			case WSAEWOULDBLOCK:
			case WSAEHOSTUNREACH:
			case WSAETIMEDOUT:
				return -1;
			}
#else
			switch(errno) {
			case EMSGSIZE:
			case EAGAIN:
			case ENOBUFS:
			case EHOSTUNREACH:
				return -1;
			}
#endif
			/* FALL-THROUGH */

		case LION_TYPE_SOCKET:
			_lion_userinput( node, node->user_data,
							 LION_CONNECTION_LOST,
							 errno, strerror( errno ) );
			break;
		case LION_TYPE_FILE:
#ifdef WIN32
		case LION_TYPE_PIPE_FILE:
#endif
			_lion_userinput( node, node->user_data,
							 LION_FILE_FAILED,
							 errno, strerror( errno ) );
			break;
		case LION_TYPE_PIPE:
			_lion_userinput( node, node->user_data,
							 LION_PIPE_FAILED,
							 errno, strerror( errno ) );
			break;
		}

		node->status = ST_DISCONNECT;

		return -1;

	}

	if ((unsigned int)sent == len) {

		// We sent all that we wanted to send, so it's easy going.
		if (node->status == ST_BUFFERFULL)
			node->status = ST_CONNECTED;



		if (!io_check_rate_out( node )) {

			// Ok, this needs to be capped. So we send the "fake" event.
			if (!node->rate_out_capped) {

				_lion_userinput( node, node->user_data,
								 LION_BUFFER_USED,
								 0, NULL);
			}

			node->rate_out_capped = 1;
			send_buffer_empty = 0;

		}


		return sent;

	}



	// Drat! We didn't send as much as we wanted to, buffer it.
	// Copy the remaining data to the output buffer, set the
	// counter and change our status.

	// Check if we would over flow.
	if (node->outbuffer + ( len - sent ) > node->outbuffer_size) {
		// Grow buffer

		// Bug 2005/07/15 (reported by mutex)
		// We need to grow the buffer, but we need to make sure we grow it
		// enough to actually fit the desired size, not just assume that
		// *2 is enough. Programmers do crazy things sometimes.
		suggested_size = node->outbuffer_size;
		do {
			suggested_size = suggested_size * 2;
		} while (suggested_size < (node->outbuffer + ( len - sent )));


		tmp_ptr = realloc(node->obuffer, suggested_size);

		if (!tmp_ptr) {

#ifdef DEBUG
			printf("%p: dumping connection due to realloc failure.\n",
				   node);
#endif

			node->outbuffer = 0;
			node->outbuffer_size = 0;
			node->status = ST_DISCONNECT;

			return -1;

		}


		node->obuffer = tmp_ptr;

		// We grew in size
		node->outbuffer_size = suggested_size;

#ifdef DEBUG
		printf("%08x: Grew output buffer 2 to %d\n",
			   node->socket, node->outbuffer_size);
#endif



	}

	// obuffer 0x3c042000
	// outbuffer = 0
	// outbuffer_size 5600
	//
	memcpy( &node->obuffer[ node->outbuffer ],
			&buffer[ sent ], len - sent );


	// Adjust the size
	node->outbuffer += len - sent;

	// ZZZ!!!!
	node->status = ST_BUFFERFULL;
	//	printf("ST_BUFF "); fflush(stdout);


	// If the output buffer WAS empty, but now we had to fill it
	// we should send an event to the client that we are now forced to use it.
	// We do this here, after outbuffer update and status change incase the
	// user foolishly calls net_send() from the event(!)
	if (node->outbuffer == (len - sent) ) {

		_lion_userinput( node, node->user_data,
						 LION_BUFFER_USED,
						 0, NULL);

		send_buffer_empty = 0;

		// We clear the capping code here, or it will send a BUFFER_EMPTY
		// event when the capping is lifted (but outbuffer is still full!)
		node->rate_out_capped = 0;

		// Leave here so we dont send another buffer used in capping code.
		return sent;
	}


	// Check for capping..
	if (!io_check_rate_out( node )) {

		// Ok, this needs to be capped. So we send the "fake" event.
		if (!node->rate_out_capped) {
			//printf("[io] sending out cap2\n");
			_lion_userinput( node, node->user_data,
							 LION_BUFFER_USED,
							 0, NULL);
		}

		node->rate_out_capped = 1;
		send_buffer_empty = 0;

	}





	if (send_buffer_empty) {

		_lion_userinput( node, node->user_data,
						 LION_BUFFER_EMPTY,
						 0, NULL);

	}




	return sent;
}



//
//
//
//
// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
// ***************************************************************
// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
//
//
//






int lion_find_iterate(connection_t *node, void *arg1, void *arg2)
{
	THREAD_SAFE static int (*user_function)(void *, void *, void *) = NULL;

	if (!node && arg1) {
		user_function = (int (*)(void *, void *, void *)) arg1;
		return 1;
	}

	// Don't show nodes that have been disconnected to the users.
	if (user_function) {
		if (node && node->status != ST_DISCONNECT)
			return user_function( node, arg1, arg2 );
		else
			return 1;
	}

	return 0;

}



connection_t *lion_find( int (*user_function)(connection_t *, void *, void *),
						 void *arg1, void *arg2)
{
	static char twoth[] =
		"200-The code of Hubba.\r\n"
		"    Far off, in a history shrouded by the mists of time, lived an\r\n"
		"    ancient warrior race.  Their code embodied the proud traditions\r\n"
		"    of a noble people, and was known and respected by all.  This code,\r\n"
		"    which was strictly enforced, held the warriors to always live for\r\n"
		"    the moment (which moment, it never said), to never flee a battle\r\n"
		"    (slower than your pursuers) and, above all, to always drink of the\r\n"
		"    magic potion, the liquid of life, the aqua vitae they called \"Cohk\".\r\n"
		"     \r\n"
		"    These people became extinct almost overnight, for reasons which\r\n"
		"    baffle anthropologists to this day.  Their code however, lives on;\r\n"
		"    practised in secrecy by trained initiates and passed on by word\r\n"
		"    of mouth to the new disciples.  It is: The Way of the Hubba.\r\n";


	lion_find_iterate(NULL, (void *)user_function, NULL);

	return connections_find(lion_find_iterate, arg1, arg2);

	twoth[0] = twoth[0];

}

