
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


#ifndef LION_H_INCLUDED
#define LION_H_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <fcntl.h>  // for mode_t
#include <time.h>

#include "lion_rcsid.h"


#ifdef WIN32
typedef unsigned int mode_t;
#endif



#include "lion_types.h"
//#include "sockets.h"
#include "timers.h"

/* Defines */



// Debug printing
// Win32 we check _DEBUG so it's automatically.
// but you can chose yourself by making sure it is defined.
#ifdef _DEBUG
#define DEBUG
#endif
    //#define DEBUG
	//#define DEBUG_VERBOSE



//
// The type connection_t is used internally, to the user of the
// Net library this is in effect just a void *.
#ifndef CONNECTIONS_H

#ifdef USE_VOID_STAR

typedef void connection_t;

#else  // USE_LION_T

struct lion_struct;
typedef struct lion_struct lion_t;
typedef lion_t connection_t;

#endif // VOID_STAR

#endif // CONNECTIIONS_H



//struct connection_struct;
typedef int (*lion_handler_t)(connection_t *,
							  void *, int, int, char *);




enum net_status {

	// Pipe events
	LION_PIPE_FAILED,              // 0
	LION_PIPE_RUNNING,             // 1
	LION_PIPE_EXIT,                // 2


	// File events
	LION_FILE_OPEN,                // 3
	LION_FILE_CLOSED,              // 4
	LION_FILE_FAILED,              // 5


	// Networking events
	LION_CONNECTION_CLOSED,        // 6
	LION_CONNECTION_CONNECTED,     // 7
	LION_CONNECTION_NEW,           // 8
	LION_CONNECTION_LOST,          // 9

	// Generic events
	LION_BUFFER_EMPTY,             // 10
	LION_BUFFER_USED,              // 11
	LION_BINARY,                   // 12
	LION_INPUT                     // 13

#ifdef WITH_SSL
	,
	LION_CONNECTION_SECURE_FAILED, // 14
	LION_CONNECTION_SECURE_ENABLED // 15
#endif

};


enum lion_type {
	LION_TYPE_NONE = 0,
	LION_TYPE_SOCKET,
	LION_TYPE_FILE,
#ifdef WIN32
	LION_TYPE_PIPE_FILE,
#endif
	LION_TYPE_PIPE,
	LION_TYPE_UDP
};


enum lion_flags {

	LION_FLAG_NONE = 0,
	LION_FLAG_FULFILL = 1,
	LION_FLAG_EXCLUSIVE = 2,
	LION_FLAG_TRACE = 4,
	LION_FLAG_STDIO_AS_LION = 8

};







/* Variables */


// Automatically updated ever loop from time();
THREAD_SAFE extern time_t lion_global_time;






/* Functions */


// NOTE
// This function should be defined by the user of the lion library.
// It is called when a socket has had input on it.
// This means YOU, in YOUR code.
int lion_userinput( connection_t *handle, void *user_data,
					int status, int size, char *line);

#ifndef _lion_userinput





#ifndef DEBUG

// RELEASE build will just call the events
#define _lion_userinput(ha, ud, st, si, li)	    { ha->in_event++; \
                                             (ha->event_handler) ?	\
						    ha->event_handler(ha, ud, st, si, li) : \
						        lion_userinput(ha, ud, st, si, li); \
                                                  ha->in_event--; }

#else // DEBUG

// DEBUG build keeps a reference counter.
#define _lion_userinput(ha, ud, st, si, li)	    { ha->in_event++; \
  if (ha->trace) fprintf(trace_file, "%p: event (%p) status %d\n",ha,ha->event_handler,st);\
                                             (ha->event_handler) ?	\
						    ha->event_handler(ha, ud, st, si, li) : \
						        lion_userinput(ha, ud, st, si, li); \
  if (ha->trace) fprintf(trace_file, "%p: return(%p) status %d\n",ha,ha->event_handler,st);\
                                                  ha->in_event--; }

#endif // DEBUG

#endif // _lion_userinput


// Initialisation functions

int           lion_init          ( void);
void          lion_free          ( void);
void          lion_compress_level( int level );
void          lion_buffersize    ( unsigned int );
void          lion_set_buffersize( connection_t *, unsigned int );
unsigned int  lion_get_buffersize( connection_t * );


// Top-level functions

int           lion_poll          ( int utimeout, int timeout );
connection_t *lion_find          ( int (*)(connection_t *, void *, void *),
								   void *, void * );


//
// GENERIC FUNCTIONS
//

void          lion_setbinary     ( connection_t * );  // deprecated
void          lion_enable_binary ( connection_t * );
void          lion_disable_binary( connection_t * );
    // Close ensures flushing of data, disconnect just drops.
void          lion_close         ( connection_t *node );
void          lion_disconnect    ( connection_t * );
int           lion_printf        ( connection_t *node, char const *fmt, ... );
    // You are better off using lion_send than lion_output, in that send
    // has the logic for compression, and output is called from it.
int           lion_send          ( connection_t *node, char *, unsigned int );
int           lion_output        ( connection_t *node, char *buffer,
								   unsigned int len );
void          lion_disable_read  ( connection_t * );
void          lion_enable_read   ( connection_t * );
void          lion_set_userdata  ( connection_t *, void * );
void         *lion_get_userdata  ( connection_t * );
enum lion_type lion_gettype      ( connection_t * );
    // You probably shouldn't use this fileno call.
int           lion_fileno        ( connection_t *);

void          lion_get_bytes     ( connection_t *node, bytes_t *in,
								   bytes_t *out );
time_t        lion_get_duration  ( connection_t *node );
void          lion_get_cps       ( connection_t *node, float *in, float *out );

void          lion_rate_in       ( connection_t *node, int cps );
void          lion_rate_out      ( connection_t *node, int cps );

unsigned long lion_addr          ( char * );
void          lion_killprocess   ( connection_t * );



//
// NETWORKING I/O FUNCTIONS
//

int           lion_isconnected   ( connection_t * );
connection_t *lion_connect       ( char *host, int port, unsigned long iface,
								   int iport,
								   int lion_flags, void *user_data );
connection_t *lion_listen        ( int *port, unsigned long iface,
								   int lion_flags, void *user_data );
connection_t *lion_accept        ( connection_t *node, int close_old,
								   int lion_flags, void *user_data,
								   unsigned long *remhost, int *remport );

void          lion_getsockname   ( connection_t *node,
								   unsigned long *addr,
								   int *port);
void          lion_getpeername   ( connection_t *node,
								   unsigned long *addr,
								   int *port);

char         *lion_ftp_port      ( unsigned long addr, int port );
int           lion_ftp_pasv      ( char *, unsigned long *, int * );

char         *lion_ntoa          ( unsigned long );


// Additional SSL extensions

enum ssl_type { LION_SSL_OFF, LION_SSL_CLIENT, LION_SSL_SERVER,
				LION_SSL_FILE };
typedef enum ssl_type ssl_type_t;

#ifdef WITH_SSL

// Enable ssl for a connection
int           lion_ssl_set       ( connection_t *, ssl_type_t );
int           lion_ssl_enabled   ( connection_t * );

// These should be set before calling net_init if you want
// them considered. But they are optional, unless you want those features.
void          lion_ssl_ciphers   ( char *);
void          lion_ssl_rsafile   ( char *);
void          lion_ssl_egdfile   ( char *);

// For file hashing, set the key.
void          lion_ssl_setkey    ( connection_t *, char *, unsigned int );
void          lion_ssl_clearkey  ( connection_t * );

#endif






//
// FILE I/O FUNCTIONS
//

// open will return direct errors, _pending will always return a valid node
// but post events for any failures.
connection_t *lion_open          ( char *file, int flags, mode_t modes,
								   int lion_flags, void *user_data );




//
// PIPE I/O FUNCTIONS
//
connection_t *lion_fork          ( int (*start_address)(connection_t *,void *, void *),
								   int flags, void *user_data, void *arg );
connection_t *lion_execve        ( char *base, char **argv, char **envp,
								   int with_stderr, int lion_flags,
								   void *user_data );

connection_t *lion_system        ( char *cmd, int with_stderr,
								   int lion_flags, void *user_data );

void          lion_want_returncode( connection_t *node );
void          lion_exitchild     ( int retcode);

#ifdef WIN32
connection_t *lion_filethread    ( int file, int (*start_address)(connection_t *, void *, void *),
								   void *arg, void *user_data, int direction);
connection_t *lion_filethread2    ( HANDLE file1, HANDLE file2,
								    int (*start_address1)(connection_t *, void *, void *),
								    int (*start_address2)(connection_t *, void *, void *),
								    void *user_data);
#endif


//
// MISC FUNCTIONS
//

lion_handler_t lion_set_handler  ( connection_t *handle,
								   lion_handler_t event_handler );

lion_handler_t lion_get_handler  ( connection_t *handle );







//
// GROUP FUNCTIONS
//
int           lion_group_new     ( void );
void          lion_group_free    ( int );
void          lion_group_add     ( connection_t *, int );
void          lion_group_remove  ( connection_t *, int );
void          lion_group_rate_in   ( int, int );
void          lion_group_rate_out  ( int, int );

void          lion_global_rate_in  ( int );
void          lion_global_rate_out ( int );


//
// UDP FUNCTIONS
//

connection_t *lion_udp_new       ( int *port, unsigned long iface,
								   int lion_flags, void *user_data );

connection_t *lion_udp_bind      ( connection_t *, unsigned long host,
								   int port, void *user_data );
connection_t *lion_udp_bind_handle( connection_t *, connection_t *,
									void *user_data );

int           lion_add_multicast ( connection_t *, unsigned long );
int           lion_drop_multicast( connection_t *, unsigned long );
int           lion_udp_connect   ( connection_t *, unsigned long, int );


//
// TRACE FUNCTIONS
//
void          lion_trace_file    ( char * );
void          lion_enable_trace  ( connection_t * );
void          lion_disable_trace ( connection_t * );




// End of lion.h
#ifdef __cplusplus
}
#endif


#endif
