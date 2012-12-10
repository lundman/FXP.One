
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

// $Id: connections.h,v 1.6 2008/12/05 01:50:33 lundman Exp $
// Temporary connection space holder
// Jorgen Lundman November 5th, 1999

#include "lion_types.h"


#ifndef CONNECTIONS_H
#define CONNECTIONS_H

#ifdef WIN32
#define WINDOWS_LEAN_AND_MEAN
#include <winsock2.h>
#endif
#include <time.h>

#ifdef WITH_SSL
#include <openssl/blowfish.h>
#endif

//#include "lion.h"



/* Defines   */

#undef ST_CONNECTED

enum connection_state {
	ST_NONE=0,
	ST_PENDING,
	ST_CONNECTED,
	ST_LISTEN,
	ST_BUFFERFULL,
	ST_READBUFFERFULL,
	ST_WANTRETURN,
	ST_RESUMEREAD,
#ifdef WITH_SSL
	ST_SSL_NEG,
#endif
	ST_DISCONNECT
};

enum net_want {
	NET_WANT_NONE = 0,
	NET_NEG_WANT_READ,   // Negotiations wants to select on read
	NET_NEG_WANT_WRITE,  //                                 write
	NET_READ_WANT_READ,  // SSL_read wants to select on read, re-call read
	NET_READ_WANT_WRITE, //                             write
	NET_WRITE_WANT_READ, // SSL_write wants to select on read
	NET_WRITE_WANT_WRITE,//                              write
	NET_WANT_CLOSE    // SECRET INTERNAL DELAYED CLOSE PROCESS.
};


enum lion_udp_flag {
	LION_UDP_UNBOUND = 0,
	LION_UDP_BOUND = 1
}; // 1, 2 , 4, 8, 16




//
// Just the default buffer size. You can set it in your aplication using
// lion_buffersize() preferably before init.
// Not on *BSD systems, if you do network traffic, you get very bad
// performance if the buffersize > MTU (generally 1500)
//
#define BUFFER_SIZE_DEFAULT 1400


// Size of the printf buffers. It is not common to printf this large.
#define STATIC_BUFFER_SIZE 8192



extern unsigned int buffer_size;
THREAD_SAFE extern FILE *trace_file;



/* Variables */

struct connection_struct {

	int type;

	/* Data fields */
	int status;
	int socket;

	int binary;

	int disable_read; // Is read-fd to be disabled? dont add to readfd
	int soft_closed; // net_close was called, but we need to flush output

	int inbuffer;  // Bytes currently held in the input buffer
	char *buffer;  // the input buffer
	int inbuffer_size; // current allocation size
	int inbinary_size; // optional buffersize in binary mode.

	int outbuffer; // Bytes currently held in the output buffer
	char *obuffer; // the output buffer
	int outbuffer_size;

#ifdef WITH_SSL

	int use_ssl;      // SSL is enabled and active
	int want_ssl;     // SSL is requested, attempt it when connected
	int auth_type;     // SSL is requested, attempt it when connected
	void *ctx;        // SSL context
	void *data_ctx;   // SSL data context

	// File encryption
	BF_KEY bf_key;     // BF_KEY
	unsigned char bf_ivec[8];
	int bf_num;

#endif

	enum net_want want_mode;

	void *user_data; // optional user data

	int rate_in;     // maximum input rate, in KB/s
	int rate_out;    // maximum input rate, in KB/s
	time_t time_start;  // Last time stamp
	bytes_t bytes_in;
	bytes_t bytes_out;

	time_t group_last;

	//	int group_bytes_in;
	//	int group_bytes_out;

	// When we are in rate_out capping, and we have posted the event, we
	// need to remember this, so we can post the available event when it
	// is within limits again.
	int rate_out_capped;

	// Groups
	// FIXME!! Multigroup support!
	int group;


	// PIPES
	int pid;           // type_pipe needs to remember pid.
	int return_code;   // and return code, if known.

	int error_code;    // We keep the error code if we delay event

#ifdef WIN32
	//HANDLE mutex;
	HANDLE event_start_in;  // "mutex" events.
	HANDLE event_start_out;
	HANDLE event_end_in;
	HANDLE event_end_out;
	int file_fd;   // Only used in Win32 as we thread to read files.
	HANDLE pipe_socket;  // Not used, but we need to close it.
	HANDLE pipe_socket2;  // Not used, but we need to close it.
	HANDLE thread;
	HANDLE thread2;
	HANDLE process;
	int (*start_address)(struct connection_struct *, void *, void *);
#endif

	// UDP
	unsigned long host;
	unsigned int port;   // we avoid the whole short int issue.

	unsigned int udp_flags; // So we know if it is bound
	unsigned long udp_host; // need to store, so we dont over write bound
	unsigned int udp_port;  // hosts when we receive data.

	// Is this node being traced?
	unsigned int trace;


	/* User defined Event Handler */
	//lion_handler_t event_handler;
	unsigned int in_event;
	int (*event_handler)(struct connection_struct *,
						 void *, int, int, char *);

	/* Internal fields */
	struct connection_struct *next;


};

typedef struct connection_struct connection_t;



/* Functions */

connection_t *connections_new(void);
void          connections_free(connection_t *);
connection_t *connections_find( int (*)(connection_t *, void *, void *),
								void *, void *);
void          connections_setuser(connection_t *, char *, char *);
void		  connections_dupe(connection_t *dst, connection_t *src);
void          connections_cycle( void );
void          connections_dump( void );

#endif






