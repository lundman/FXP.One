
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

// $Id: udp.c,v 1.3 2009/08/10 03:44:49 lundman Exp $
// UDP functionality
// Jorgen Lundman April 22nd, 2003.

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <errno.h>

#include "connections.h"
#include "sockets.h"
#include "lion.h"



#ifndef WIN32
#include <unistd.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#endif



//
// Create a new udp socket...
// If port is set (!0) we attempt to open that port, otherwise, it
// will pick first available. "iface" is optional bind ip.
// flags can be FULFILL.
//
connection_t *lion_udp_new(int *port, unsigned long iface,
						   int lion_flags, void *user_data)
{
	connection_t *newd;

	newd = connections_new();

	newd->type = LION_TYPE_UDP;
	newd->user_data = user_data;

	if (lion_flags & LION_FLAG_TRACE)
		lion_enable_trace(newd);

	if (newd->trace)
		fprintf(trace_file, "%p: lion_udp_new\n", newd);

	newd->socket = sockets_udp(port, iface);

#ifdef DEBUG
	printf("[udp] new opened %d port %d\n", newd->socket, *port);
#endif

	if (newd->socket == -1) {

		newd->status = ST_PENDING;
		newd->error_code = errno;

		if (lion_flags & LION_FLAG_FULFILL) {

			return newd;

		}

		_lion_userinput( newd, newd->user_data, LION_CONNECTION_LOST,
						 errno, strerror( errno ) );

		connections_free(newd);

		return NULL;

	}



	newd->status = ST_PENDING;

	return newd;

}




//
// "bind" a host and port to a UDP connection. Note that this isn't a permanent
// bind if this port also receives "anonymous" incoming packets ie if
// user_data = NULL. (Then the host/port is changed to that. And it returns
// NULL.
//
// If this function is called with a new user_data, it will allocate a new
// connection_t to deal ONLY with data to/from the host&port pair. No
// anonymous/unregistered data will be received.
//
connection_t *lion_udp_bind(connection_t *node, unsigned long host, int port,
							void *user_data)
{
	connection_t *newd;

	if (node->trace)
		fprintf(trace_file, "%p: lion_udp_bind(%p)\n", node, user_data);

	if (!user_data) {

#ifdef DEBUG
		printf("[udp] bound for %08lX:%d\n", host, port);
#endif

		node->host = ntohl(host);
		node->port = ntohs(port);

		return NULL;

	}


	// TODO - create new node, set up to ONLY receive data from it
	newd = connections_new();

	connections_dupe( newd, node );

	// Assign unique user_data
	newd->user_data = user_data;

	// bind it. We could call ourselves with a NULL user_data, but how tedious
#ifdef DEBUG
	printf("[udp] dupe-bound for %08lX:%d\n", host, port);
#endif

	newd->host = ntohl(host);
	newd->port = ntohs(port);

	newd->udp_flags |= LION_UDP_BOUND;

	return newd;

}




//
// Alternative function to lion_udp_bind(). Works exactly the same
// but takes a handle instead.
//
connection_t *lion_udp_bind_handle(connection_t *node, connection_t *remote,
							void *user_data)
{

	if (!node || !remote)
		return NULL;


	return lion_udp_bind(node, ntohl(remote->host),
						 (int)ntohs((short int)remote->port), user_data);

}












int udp_find_others_sub(connection_t *handle, void *arg1, void *arg2)
{
	connection_t *us = (connection_t *)arg1;

	if (!us) return 0; // stop, invalid situation

	if (handle == us) return 1; // keep going.

	// Next one technically is not required since fd are unique.
	if (handle->type != LION_TYPE_UDP) return 1; // keep going

#ifdef DEBUG_VERBOSE
	printf("[udp] comparing %d == %d\n", handle->socket, us->socket);
#endif

	if (handle->socket == us->socket) return 0; // there is another!

	return 1; // keep looking
}





//
// Check if there are other UDP nodes, with the same socket fd.
//
connection_t *udp_find_others(connection_t *us)
{
	//	printf("[udp] no others?\n");
	return connections_find(udp_find_others_sub, (void *) us, NULL);

}




int udp_input_sub(connection_t *handle, void *arg1, void *arg2)
{
	connection_t *data = (connection_t *) arg1;
	connection_t **anonymous = (connection_t **) arg2;

	if (!data) return 0; // Just a failure, just quit.

	if (handle->type != LION_TYPE_UDP) return 1; // Not UDP, keep going

	if (data->socket != handle->socket) return 1; // Not same socket...


	if (handle->udp_flags & LION_UDP_BOUND) {

#ifdef DEBUG_VERBOSE
		printf(" [udp] bound testing %08lX:%d\n",
			   handle->host, ntohs(handle->port));
#endif

		if (data->udp_host == handle->host) {  // same host

			if (data->udp_port == handle->port) { // same port

#ifdef DEBUG_VERBOSE
				printf("[udp] input_sub matched..\n");
#endif
				return 0; // Found a perfect match!

			}
		}

	} else { // unbound is an anonymous port

		// Unbound udp, if anonymous is passed along, set it
		if (anonymous)
			*anonymous = handle;

	}

	return 1;

}


//
// We have received input from udp, but we need to find with udp packet is
// the one we should deliver this for. Basically, look up up the host name
// in our "cache", and match if there is one. If not, just deliver to
// the node that is not bound, (0.0.0.0) and input is only received on
// a node that is unbound.
//
void udp_input(connection_t *input, int status, int size, char *buff)
{
	connection_t *anonymous = NULL;
	connection_t *node;
	// See if it is bound..

#ifdef DEBUG_VERBOSE
	printf(" [udp] input from %08lX:%d\n", input->udp_host,
		   ntohs(input->udp_port));
#endif

	// No? Just deliver as anonymous

	// No hash-cache yet, just look it up
	node = connections_find(udp_input_sub, (void *)input, (void *)&anonymous);


#ifdef DEBUG_VERBOSE
	printf(" [udp] input node=%08lX anonymous=%08lX\n", node, anonymous);
#endif

	// If we got a node we have a perfect match, send it it...

	if (node) {

		_lion_userinput( node, node->user_data,
						 status, size, buff);


		return;
	}


	// No match, then if anonymous is set, use it
	if (anonymous) {

		// Assigned over where the data come from, since it is anonymous
		anonymous->host = input->udp_host;
		anonymous->port = input->udp_port;

		_lion_userinput( anonymous, anonymous->user_data,
						 status, size, buff);

		return;
	}

	// We failed to match, shouldn't happen.
#ifdef DEBUG
	printf("[udp] input: no anonymous node to handle data from %08lX:%d -> %d bytes\n",
		   input->udp_host, input->udp_port, size);
#endif

}


int lion_udp_connect(connection_t *node, unsigned long host, int port)
{
    if (!node) return -1;
    return sockets_connect_udp(node->socket, host, port);
}
