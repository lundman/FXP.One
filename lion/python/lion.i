%module lion
%header %{
#include "lion.h"
%}
%rename(lion_set_handler) lion_set_handler_python;
%inline %{

lion_handler_t lion_set_handler_python(lion_t *handle, PyObject *event_handler){
   PyObject *old=NULL;

   if (event_handler != NULL){
      if(!PyCallable_Check(event_handler)){
         PyErr_SetString(PyExc_TypeError, "parameter must be callable");
         return NULL;
      }
      Py_XINCREF(event_handler);
      old = (PyObject *)lion_set_handler(handle,(lion_handler_t)event_handler);
      Py_XDECREF(old);
      return (lion_handler_t)old;
   }
   PyErr_SetString(PyExc_ValueError,"parameter must not be null");
   return NULL;
}

int lion_userinput( lion_t *handle, void *user_data, int status, int size, char *line){
   PyObject *py_handle;
   PyObject *result;
   PyObject *arglist;

	// py_handle = SWIG_NewPointerObj((handle), SWIGTYPE_p_lion_t, 0);
   py_handle = SWIG_NewPointerObj((handle), SWIGTYPE_p_lion_t, 0);
   arglist = Py_BuildValue("N s i i s",py_handle,user_data,status,size,line);
   result = PyEval_CallObject((PyObject*)lion_get_handler(handle),arglist);
   Py_DECREF(py_handle);
   Py_DECREF(arglist);
   if (result == NULL){
      return (int)NULL;
   }else{
      Py_DECREF(result);
      return 0;
   }
}

%}

typedef int (*lion_handler_t)(lion_t *, 
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
	LION_TYPE_PIPE,
	LION_TYPE_UDP
};

enum lion_flags {

	LION_FLAG_NONE = 0,
	LION_FLAG_FULFILL = 1,
	LION_FLAG_EXCLUSIVE = 2

};

// Automatically updated ever loop from time();
extern time_t lion_global_time;

#ifndef _lion_userinput
#ifndef DEBUG
// RELEASE build will just call the events
#define _lion_userinput(ha, ud, st, si, li)  (ha->event_handler) ?   \
                  ha->event_handler(ha, ud, st, si, li) : \
                  lion_userinput(ha, ud, st, si, li);
#else // DEBUG
// DEBUG build keeps a reference counter.
#define _lion_userinput(ha, ud, st, si, li)      { ha->in_event++; \
                                             (ha->event_handler) ?   \
                      ha->event_handler(ha, ud, st, si, li) : \
                          lion_userinput(ha, ud, st, si, li); \
                                                  ha->in_event--; }
#endif // DEBUG
#endif // _lion_userinput

// Initialisation functions

extern int           lion_init          ( void);
extern void          lion_free          ( void);
extern void          lion_compress_level( int level );
extern void          lion_buffersize    ( unsigned int );

// Top-level functions

extern int           lion_poll          ( int utimeout, int timeout );
extern lion_t *lion_find          ( int (*)(lion_t *, void *, void *),
								   void *, void * );

//
// GENERIC FUNCTIONS
//

extern void          lion_setbinary     ( lion_t * );
    // Close ensures flushing of data, disconnect just drops.
extern void          lion_close         ( lion_t *node );
extern void          lion_disconnect    ( lion_t * );
extern int           lion_printf        ( lion_t *node, char const *fmt, ... );
    // You are better off using lion_send than lion_output, in that send
    // has the logic for compression, and output is called from it.
extern int           lion_send          ( lion_t *node, char *, unsigned int );
extern int           lion_output        ( lion_t *node, char *buffer, 
								   unsigned int len );
extern void          lion_disable_read  ( lion_t * );
extern void          lion_enable_read   ( lion_t * );
extern void          lion_set_userdata  ( lion_t *, void * );
extern void         *lion_get_userdata  ( lion_t * );
extern enum lion_type lion_gettype      ( lion_t * );
    // You probably shouldn't use this fileno call.
extern int           lion_fileno        ( lion_t *);

extern void          lion_get_bytes     ( lion_t *node, bytes_t *in, 
								   bytes_t *out );
extern time_t        lion_get_duration  ( lion_t *node );
extern void          lion_get_cps       ( lion_t *node, float *in, float *out );

extern void          lion_rate_in       ( lion_t *node, int cps );
extern void          lion_rate_out      ( lion_t *node, int cps );

extern unsigned long lion_addr          ( char * );

//
// NETWORKING I/O FUNCTIONS
//

extern int           lion_isconnected   ( lion_t * );
extern lion_t *lion_connect       ( char *host, int port, unsigned long iface,
								   int iport,
								   int lion_flags, void *user_data );
extern lion_t *lion_listen        ( int *port, unsigned long iface, 
								   int lion_flags, void *user_data );
extern lion_t *lion_accept        ( lion_t *node, int close_old,
								   int lion_flags, void *user_data,
								   unsigned long *remhost, int *remport );

extern void          lion_getsockname   ( lion_t *node, 
								   unsigned long *addr, 
								   int *port);
extern void          lion_getpeername   ( lion_t *node, 
								   unsigned long *addr, 
								   int *port);

extern char         *lion_ftp_port      ( unsigned long addr, int port );
extern int           lion_ftp_pasv      ( char *, unsigned long *, int * );

extern char         *lion_ntoa          ( unsigned long );

// Additional SSL extensions

enum ssl_type { LION_SSL_OFF, LION_SSL_CLIENT, LION_SSL_SERVER, 
				LION_SSL_FILE };
typedef enum ssl_type ssl_type_t;

#ifdef WITH_SSL

// Enable ssl for a connection
extern int           lion_ssl_set       ( lion_t *, ssl_type_t );
extern int           lion_ssl_enabled   ( lion_t * );

// These should be set before calling net_init if you want
// them considered. But they are optional, unless you want those features.
extern void          lion_ssl_ciphers   ( char *);
extern void          lion_ssl_rsafile   ( char *);
extern void          lion_ssl_egdfile   ( char *);

// For file hashing, set the key.
extern void          lion_ssl_setkey    ( lion_t *, char *, unsigned int );
extern void          lion_ssl_clearkey  ( lion_t * );

#endif

//
// FILE I/O FUNCTIONS
//

// open will return direct errors, _pending will always return a valid node
// but post events for any failures.
extern lion_t *lion_open          ( char *file, int flags, mode_t modes, 
								   int lion_flags, void *user_data );

//
// PIPE I/O FUNCTIONS
//
extern lion_t *lion_fork          ( int (*start_address)(lion_t *,void *, void *),
								   int flags, void *user_data, void *arg );
extern lion_t *lion_execve        ( char *base, char **argv, char **envp,
								   int with_stderr, int lion_flags, 
								   void *user_data );

extern lion_t *lion_system        ( char *cmd, int with_stderr,
								   int lion_flags, void *user_data );

extern void          lion_want_returncode( lion_t *node );
extern void          lion_exitchild     ( int retcode);

//
// MISC FUNCTIONS
//

extern lion_handler_t lion_set_handler  ( lion_t *handle, 
								   lion_handler_t event_handler );

extern lion_handler_t lion_get_handler  ( lion_t *handle );

//
// GROUP FUNCTIONS
//
extern int           lion_group_new     ( void );
extern void          lion_group_free    ( int );
extern void          lion_group_add     ( lion_t *, int );
extern void          lion_group_remove  ( lion_t *, int );
extern void          lion_group_rate_in   ( int, int );
extern void          lion_group_rate_out  ( int, int );

extern void          lion_global_rate_in  ( int );
extern void          lion_global_rate_out ( int );

//
// UDP FUNCTIONS
//

extern lion_t *lion_udp_new       ( int *port, unsigned long iface,
								   int lion_flags, void *user_data );

extern lion_t *lion_udp_bind      ( lion_t *, unsigned long host,
								   int port, void *user_data );
extern lion_t *lion_udp_bind_handle( lion_t *, lion_t *,
									void *user_data );

extern int           lion_add_multicast ( lion_t *, unsigned long );
extern int           lion_drop_multicast( lion_t *, unsigned long );

