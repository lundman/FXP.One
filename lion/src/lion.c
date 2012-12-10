
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

// $Id: lion.c,v 1.11 2008/12/05 01:50:33 lundman Exp $
// Wrapper calls for the lion library
// Jorgen Lundman January 21st, 2003


// Universal system headers
#if HAVE_CONFIG_H
#include <config.h>
#endif


#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <errno.h>
#include <signal.h>
#include <string.h>

// OS specific system headers
#ifdef WIN32
#define WINDOWS_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#else

#include <arpa/inet.h>
#include <sys/time.h> /* big sigh to linux */
#include <unistd.h>
#include <netinet/in.h>
#include <sys/resource.h>
#include <sys/socket.h>
#endif
#include <fcntl.h>


// Implementation specific headers

#include "connections.h"
#include "sockets.h"
#include "io.h"

#include "lion.h"
#include "pipe.h"
#include "lgroup.h"

#ifdef WITH_SSL
#include "tls.h"
#endif

#include "timers.h"


__RCSID("$LiON: lundman/lion/src/lion.c,v 1.11 2008/12/05 01:50:33 lundman Exp $");




#ifndef MIN
#define MIN(a,b) (((a)<(b))?(a):(b))
#endif

#ifndef EWOULDBLOCK
#define EWOULDBLOCK EAGAIN
#endif












int lion_init(void)
{
	int ret;
	// Lets set some signal that we may wish to ignore
#ifndef WIN32
	signal(SIGPIPE, SIG_IGN);
#endif


#ifndef WIN32
	{
		struct rlimit rlim;

		getrlimit(RLIMIT_NOFILE, &rlim);

		// For some reason Solaris don't want to go beyong 4096.
		rlim.rlim_cur = MIN(rlim.rlim_max, 4096);

		setrlimit(RLIMIT_NOFILE, &rlim);

	}
#endif

#ifdef DEBUG
	printf("[main] Max open file's: %d\n", getdtablesize());
#endif


	// Lets set the global time now too, so that the apps can start using it.
	time(&lion_global_time);


	ret = sockets_init();

	if (ret)
		return ret;

#ifdef WITH_SSL
	ret = tls_init();
#endif

	if (!trace_file) trace_file = stderr;


	timers_init();

	return ret;

}


void lion_free(void)
{

	// If we have connections, perhaps loop through them and free them?
	io_release_all( NULL );

	timers_free();

#ifdef WITH_SSL

	tls_free();

#endif

	lgroup_exit();

	sockets_exit();

	if (trace_file != stderr) fclose(trace_file);

}




//
// Allow application to set the buffer size, slightly advanced mode.
// Note we don't do any sanity checking here, so if they really want
// -1, 0 or M_INFINITY, they can do that.
void lion_buffersize(unsigned int size)
{

	buffer_size = size;

#ifdef DEBUG
	printf("[lion] new buffersize %u\n", size);
#endif


}



//
// Set buffersize "preferred" in binary mode. This means you will get data
// of size _no more_ than "size".
// Size can not be larger than global buffersize set above.
// Size of 0 resets it to default.
//
void lion_set_buffersize(connection_t *node, unsigned int size)
{

	if ((!size) ||
		(size > buffer_size))
		node->inbinary_size = 0;
	else
		node->inbinary_size = size;

	if (node->trace)
		fprintf(trace_file, "%p: lion_set_buffersize(%d)\n", node,
				node->inbinary_size);

}

unsigned int lion_get_buffersize(connection_t *node)
{

	if (node->trace)
		fprintf(trace_file, "%p: lion_get_buffersize()\n", node);

	if (!node->inbinary_size)
		return buffer_size;

	return node->inbinary_size;

}







void lion_getsockname(connection_t *node, unsigned long *addr,
					  int *port)
{

	if (!node || (node->socket < 0))
		return;

	if (node->trace)
		fprintf(trace_file, "%p: lion_getsockname\n", node);

	if (addr)
		*addr = ntohl(sockets_getsockname( node->socket, port ));

}



void lion_getpeername(connection_t *node, unsigned long *addr,
					  int *port)
{
	unsigned short int pport;

	if (!node || (node->socket < 0))
		return;

	if (!addr)
		return;

	if (node->trace)
		fprintf(trace_file, "%p: lion_getpeername\n", node);

	// Not already known, look it up
	if (!node->host) {
		*addr = ntohl(sockets_getpeername( node->socket, port ));
		return;
	}

	// If we already know it, just pass it back, in host order.
	*addr = ntohl(node->host);
	if (port) {
		pport = node->port;
		*port = (int) ntohs( pport );
	}
}


char *lion_ftp_port( unsigned long addr, int port )
{

	return sockets_port( ntohl(addr), (int)ntohs((unsigned short)port));

}

int lion_ftp_pasv( char *line, unsigned long *addr, int *port )
{
	int i;
	unsigned short int pport;

	if (line && *line && addr && port) {

		i = sockets_pasv(line, addr, &pport);
		*addr = ntohl(*addr);
		*port = (int) ntohs(pport);
		return i;
	}

	return 0;

}


char *lion_ntoa( unsigned long addr )
{

	return ul2a( ntohl( addr ));

}



void lion_setbinary(connection_t *node)
{
	//	int i;

	if (!node)
		return;

	node->binary ^= 1;

	if (node->trace)
		fprintf(trace_file, "%p: lion_setbinary (%d)\n", node, node->binary);

#if 0
	if (node->binary) {

		i = buffer_size;
		i = setsockopt(node->socket,
					   SOL_SOCKET, SO_RCVLOWAT, &i, sizeof(i));
		//if (i) perror("setsockopt");

		//printf("[lion] set RCVLOWAT to %d:%d\n", buffer_size, i);

	} else {

		i = 1;
		i = setsockopt(node->socket,
					   SOL_SOCKET, SO_RCVLOWAT, &i, sizeof(i));

		//printf("[lion] set RCVLOWAT to 1:%d\n", i);

	}
#endif


}




void lion_enable_binary(connection_t *node)
{
	//	int i;

	if (!node)
		return;

	node->binary = 1;

	if (node->trace)
		fprintf(trace_file, "%p: lion_enable_binary\n", node);

}


void lion_disable_binary(connection_t *node)
{
	//	int i;

	if (!node)
		return;

	node->binary = 0;

	if (node->trace)
		fprintf(trace_file, "%p: lion_disable_binary\n", node);

}



int lion_fileno(connection_t *node)
{

	if (!node)
		return -1;

	if (node->trace)
		fprintf(trace_file, "%p: lion_fileno\n", node);

#ifdef WIN32
	if (node->type == LION_TYPE_PIPE_FILE)
		return node->file_fd;
#endif

	return node->socket;

}



int lion_isconnected(connection_t *node)
{

	if (!node)
		return 0;

	if (node->trace)
		fprintf(trace_file, "%p: lion_isconnected (%d)\n", node, node->status);

	switch (node->type) {

	case LION_TYPE_SOCKET:

		switch (node->status) {

		case ST_CONNECTED:
		case ST_BUFFERFULL:
		case ST_READBUFFERFULL:
			return 1;
		default:
			return 0;
		}

		break;

	default: // Not socket
		return 0;
	}

}

void lion_disable_read( connection_t *node )
{

	if (node->trace)
		fprintf(trace_file, "%p: lion_disable_read\n", node);

	node->disable_read = 1;

#ifdef DEBUG_VERBOSE
	printf("read disabled on %p\n", node);
#endif

}


void lion_enable_read ( connection_t *node )
{

	if (node->trace)
		fprintf(trace_file, "%p: lion_enable_read\n", node);

	node->disable_read = 0;

	// Do we still have data around from last time?
	if (node->inbuffer) {
		if (node->status == ST_CONNECTED)
			node->status = ST_RESUMEREAD;

		io_force_loop = 1;
	}
#ifdef DEBUG_VERBOSE
	printf("read enabled on %p\n", node);
#endif

}



//
// Hidden function. There should really be no need to support this API
// call anymore. It used to be used for TLS, but that is all not internal
// to lion. I have left it here incase we find it is required again in
// future.
//
void lion_want_select( connection_t *node, enum net_want want)
{

	if (!node)
		return;

	if (node->trace)
		fprintf(trace_file, "%p: lion_want_select\n", node);


	node->want_mode = want;

	//	printf("want set %d\n", node->want_mode);
}



//
// Fetch the nodes type
//
enum lion_type lion_gettype( connection_t *node )
{

	if (!node)
		return LION_TYPE_NONE;

	if (node->trace)
		fprintf(trace_file, "%p: lion_gettype\n", node);

	return node->type;

}


//
// Set user_data of a node
// this should probably return the old user_data.
//
void lion_set_userdata( connection_t *node, void *user_data )
{

	if (!node)
		return;

	if (node->trace)
		fprintf(trace_file, "%p: lion_set_userdata (%p)\n", node, user_data);

	node->user_data = user_data;

}

void *lion_get_userdata( connection_t *node )
{

	if (!node)
		return NULL;

	if (node->trace)
		fprintf(trace_file, "%p: lion_get_userdata (%p)\n", node,
				node->user_data);

	return node->user_data;

}


lion_handler_t lion_set_handler( connection_t *handle, lion_handler_t event_handler )
{

	lion_handler_t old;

	if (handle->trace)
		fprintf(trace_file, "%p: lion_set_handler (%p)\n",
				handle, event_handler);

	old = handle->event_handler;
	handle->event_handler = event_handler;

	return old;

}


lion_handler_t lion_get_handler( connection_t *handle )
{

	if (!handle)
		return (lion_handler_t) lion_userinput;

	if (handle->trace)
		fprintf(trace_file, "%p: lion_get_handler (%p)\n", handle,
				handle->event_handler);

	return handle->event_handler ? handle->event_handler : lion_userinput;

}


connection_t *lion_adopt(int fd, enum lion_type type, void *user_data)
{
	connection_t *newd;

	if (fd < 0) return NULL; // people are dumb sometimes

	if ((type < LION_TYPE_NONE) ||
		(type > LION_TYPE_PIPE)) return NULL;


	newd = connections_new();

	newd->type = type;
	newd->user_data = user_data;

	newd->status = ST_PENDING; // Should issue CONNECTED

	io_force_loop = 1;

	return newd;

}



void lion_rate_in(connection_t *node, int cps)
{

	if (node->trace)
		fprintf(trace_file, "%p: lion_rate_in (%d)\n", node, cps);

	node->rate_in = cps;

}

void lion_rate_out(connection_t *node, int cps)
{

	if (node->trace)
		fprintf(trace_file, "%p: lion_rate_out (%d)\n", node, cps);

	node->rate_out = cps;

}




void lion_get_bytes(connection_t *node, bytes_t *in, bytes_t *out)
{
	if (!node) return;

	if (node->trace)
		fprintf(trace_file, "%p: lion_get_bytes\n", node);

	if (in)
		*in = node->bytes_in;
	if (out)
		*out = node->bytes_out;
}




time_t lion_get_duration(connection_t *node)
{
	if (!node) return 0;

	if (node->trace)
		fprintf(trace_file, "%p: lion_get_duration\n", node);

	return lion_global_time - node->time_start;
}




void lion_get_cps(connection_t *node, float *in, float *out)
{

	// If no time has passed, just return.
	if (node->time_start >= lion_global_time)
		return;

	if (node->trace)
		fprintf(trace_file, "%p: lion_get_cps\n", node);

	if (in) {

		*in = 0.0;

		// Check bytes has transferred.
		if (node->bytes_in) {

			*in = ( (float) (node->bytes_in) /
					(float) (lion_global_time - node->time_start) ) /
				(float)1024.0;
		}

	}

	if (out) {

		*out = 0.0;

		// Check bytes has transferred.
		if (node->bytes_out) {

			*out = ( (float) (node->bytes_out) /
					 (float) (lion_global_time - node->time_start) ) /
				(float)1024.0;
		}

	}


}



void lion_killprocess(connection_t *node)
{
	if (node->trace)
		fprintf(trace_file, "%p: lion_killprocess (%d)\n", node, node->status);
#ifdef WIN32
	if (node->thread) {
		printf("lion_killprocess: TerminateProcess %p: %d\n",
			   node, node->thread);
		TerminateProcess(node->thread, 1);
	}
#else
	if (node->pid) {
		printf("lion_killprocess: kill %p: %d\n", node, node->pid);
		kill(SIGKILL, node->pid);
	}
#endif
}




void lion_exitchild(int retcode)
{
#ifdef WIN32
	ExitThread( retcode );
#else
	_exit( retcode );
#endif
}











//
// Change a socket's state to (possibly) attempt SSL/TLS. If we are
// already in CONNECTED state, we act immediately. Otherwise we simply
// delay and in the logic engine where we would go into state CONNECTED
// we act.
//
int lion_ssl_set( connection_t *node, ssl_type_t type)
{
#ifdef WITH_SSL
	int ret;

	if (node->trace)
		fprintf(trace_file, "%p: lion_ssl_set (%d)\n", node, type);

	switch( type ) {

	case LION_SSL_SERVER:  // Attempt SSL/TLS as a server side.

		if (!net_server_SSL)  // Did we init ok for server?
			return -3;

		// If the socket is already connected, we can kick it in now,
		// otherwise we delay

		if (node->status == ST_CONNECTED) {

			//			printf("Already connected, starting tls_auth\n");

			// Start SSL/TLS process
			ret = tls_auth( node );

			return ret;
		}

		// Delay
		node->want_ssl = LION_SSL_SERVER;
		//		printf("Ok setting delay on for %p - %d\n", node, node->status );


		return 1; // in progress ?

	case LION_SSL_CLIENT:
		if (!net_client_SSL)  // Did we init ok for client?
			return -4;


		if (node->status == ST_CONNECTED) {

			ret = tls_clauth( node );

			return ret;

		}


		node->want_ssl = LION_SSL_CLIENT;

		return 1; // in progress ?


	case LION_SSL_OFF:
		return 0;




	case LION_SSL_FILE:

		if ((node->type != LION_TYPE_FILE)
#ifdef WIN32
			&& (node->type != LION_TYPE_PIPE_FILE)
#endif
			)
			return -5;

		node->use_ssl = 1;
		break;


	}

#endif
	return -99;
}


int lion_ssl_enabled( connection_t *node)
{

#ifdef WITH_SSL
	if (node->trace)
		fprintf(trace_file, "%p: lion_ssl_enabled (%d)\n", node, node->use_ssl);

	if (node->use_ssl)
		return 1;
#endif

	return 0;

}




void lion_ssl_ciphers( char *value)
{
#ifdef WITH_SSL
	if (value && *value)
		ssl_tlsciphers = strdup(value);
#endif
}

void lion_ssl_rsafile( char *value)
{
#ifdef WITH_SSL
	if (value && *value)
		ssl_tlsrsafile = strdup(value);
#endif
}

void lion_ssl_egdfile( char *value)
{
#ifdef WITH_SSL
	if (value && *value)
		ssl_egdsocket = strdup(value);
#endif
}



void lion_ssl_clearkey(connection_t *handle)
{

#ifdef WITH_SSL

	if (!handle) return;

	if (handle->trace)
		fprintf(trace_file, "%p: lion_clear_key\n", handle);

	memset(&handle->bf_key, -1, sizeof(handle->bf_key));
	memset(&handle->bf_key,  0, sizeof(handle->bf_key));

#endif

}


void lion_ssl_setkey( connection_t *handle, char *key, unsigned int len)
{
#ifdef WITH_SSL
	//	char ivec[8] = { 0,0,0,0,0,0,0,0 };

	if (!handle)
		return;

	if (handle->trace)
		fprintf(trace_file, "%p: lion_ssl_setkey (%d)\n",
				handle, len);

	lion_ssl_clearkey(handle);

	if (!key)
		return;


	// We hash in key, and then hash in a static buffer.
	// Not really much more secure but at least the key wont just be
	// in plain view in cores, or gdb.
	BF_set_key(&handle->bf_key, len, (unsigned char *)key);

	// Clear ivec (assumes we are at the start of the file
	memset(&handle->bf_ivec, 0, sizeof(handle->bf_ivec));
	handle->bf_num = 0;


#endif
}










//
// group wrappers
//

int lion_group_new(void)
{
	return lgroup_new();
}

void lion_group_free(int gid)
{
	lgroup_free(gid);
}

void lion_group_add(connection_t *handle, int gid)
{
	if (handle->trace)
		fprintf(trace_file, "%p: lion_group_add (%d)\n", handle, gid);

	lgroup_add(handle, gid);
}

void lion_group_remove(connection_t *handle, int gid)
{
	if (handle->trace)
		fprintf(trace_file, "%p: lion_group_remove (%d)\n", handle, gid);

	lgroup_remove(handle, gid);
}

void lion_group_rate_in(int gid, int cps)
{
	// Lets not let them set global this way
	if (gid == LGROUP_GLOBAL)
		return;

	lgroup_set_rate_in(gid, cps);

}

void lion_group_rate_out(int gid, int cps)
{
	// Lets not let them set global this way
	if (gid == LGROUP_GLOBAL)
		return;

	lgroup_set_rate_out(gid, cps);

}

void lion_global_rate_in(int cps)
{
	lgroup_set_rate_in(LGROUP_GLOBAL, cps);
}

void lion_global_rate_out(int cps)
{
	lgroup_set_rate_out(LGROUP_GLOBAL, cps);
}









unsigned long lion_addr(char *str)
{

	if (!str) return 0L;

	return ntohl(inet_addr(str));

}



int lion_add_multicast(connection_t *node, unsigned long maddr)
{
	struct ip_mreq mreq;
	int rc;


	if (node->trace)
		fprintf(trace_file, "%p: lion_add_multicast\n", node);

	/* join multicast group */
	mreq.imr_multiaddr.s_addr=htonl(maddr);
	mreq.imr_interface.s_addr=htonl(INADDR_ANY);

	rc = setsockopt(node->socket,
					IPPROTO_IP,IP_ADD_MEMBERSHIP,
					(void *) &mreq, sizeof(mreq));
	return rc;

}

int lion_drop_multicast(connection_t *node, unsigned long maddr)
{
	struct ip_mreq mreq;
	int rc;

	if (node->trace)
		fprintf(trace_file, "%p: lion_drop_multicast\n", node);

	/* join multicast group */
	mreq.imr_multiaddr.s_addr=htonl(maddr);
	mreq.imr_interface.s_addr=htonl(INADDR_ANY);

	rc = setsockopt(node->socket,
					IPPROTO_IP,IP_DROP_MEMBERSHIP,
					(void *) &mreq, sizeof(mreq));

	return rc;

}




//
// For the trace feature, allow API to set a tracefile
//
// If we wanted to be crazy, we could take a lion_t instead, so they
// use lion_open or anything, like a socket? Heh
//

void lion_trace_file(char *name)
{
	FILE *fd;

	fd = fopen(name, "a");

	if (fd) {
		trace_file = fd;
		fprintf(trace_file, "\nLiON trace file started\n");
#ifndef WIN32
		setvbuf(trace_file, NULL, _IOLBF, 0);
#endif
	}

}

void lion_enable_trace(connection_t *node)
{
	node->trace = 1;
	fprintf(trace_file, "%p: trace enabled\n", node);
}

void lion_disable_trace(connection_t *node)
{
	node->trace = 0;
	fprintf(trace_file, "%p: trace disabled\n", node);
}

