
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

// $Id: sockets.c,v 1.10 2011/06/13 06:23:42 lundman Exp $
// Lowest level socket manipulation routines.
// Jorgen Lundman October 10, 1999

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <time.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>



#ifndef WIN32

#include <unistd.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#else

// Unfortunately winsock.h includes windows.h, so we try to get as little
// of it as possible.
#define WINDOWS_LEAN_AND_MEAN
#include <winsock2.h>
#define HAVE_STDARG_H 1

static int sockets_getdtablesize = 65535;

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


// Some OS's don't define this, notibly SunOS 4 and below.
#ifndef INADDR_NONE
#define INADDR_NONE -1
#endif


#include "sockets.h"
#include "lion_rcsid.h"


#include "connections.h"


__RCSID("$LiON: lundman/lion/src/sockets.c,v 1.10 2011/06/13 06:23:42 lundman Exp $");


// There isn't much sense in not having this on in lion.
#define NONBLOCKING_SOCKETS


#ifdef EADDRINUSE // See sockets_connect()
THREAD_SAFE static int recursion_reentry = 0;
#endif


// One-time initialisations required. (buffers, lists etc)
// These really just stubs incase of future need.
int sockets_init(void)
{



#ifdef WIN32
  // Only WIN32 needs to do something here
  // This is straight out of
  WORD wVersionRequested;                               // WinSock specific
  WSADATA wsaData;					// WinSock specific
  int result;

  wVersionRequested = MAKEWORD( 2, 2 );

  result = WSAStartup( wVersionRequested, &wsaData );

  if ( result ) {
    printf("Couldn't start WinSock :( \n");
    exit(1);
  }

  printf("WinSock version %d.%d Max %d\n",
	 LOBYTE( wsaData.wVersion ), HIBYTE( wsaData.wVersion ),
	 wsaData.iMaxSockets);


  if (wsaData.iMaxSockets > sockets_getdtablesize)
    sockets_getdtablesize = (unsigned int) wsaData.iMaxSockets;

#endif


  return 0;

}




#ifdef WIN32
// Windows doesn't define this function, but we need it to compute
// maximum allowed open sockets.
int getdtablesize( void )
{

  return sockets_getdtablesize;

}
#endif


// Opposite to sockets_init.
void sockets_exit(void)
{


#ifdef WIN32
  WSACleanup( );
#endif


}

int sockets_setnonblocking(int sock)
{
	int i;

	// Set non blocking
#ifdef WIN32
	i = 1;
	if (ioctlsocket(sock, FIONBIO, &i) == SOCKET_ERROR) {
		sockets_close(sock);
		return -1;
	}

#else

  i = fcntl(sock, F_GETFL);
  if (i < 0) {
	  perror("fnctl F_GETFL");
	  close(sock);
	  return -1;
  }

  i |= O_NONBLOCK;

  if (fcntl(sock,F_SETFL, i) < 0) {
	  perror("fnctl F_SETFL O_NONBLOCK");
	  close(sock);
	  return -1;
  }

#ifdef DEBUG
  i = fcntl(sock, F_GETFL);
  if ((i < 0) || !(i & O_NONBLOCK)) {
	  perror("fnctl F_GETFL check");
	  close(sock);
	  return -1;
  }
  printf("[sockets] fd %d verified nonblocking %d\n", sock, i);
#endif


#endif

  return sock;

}


// Close an already open socket.
//
void sockets_close(int old_socket)
{
#ifdef WIN32
	closesocket((SOCKET)old_socket);
#else
	close(old_socket);
#endif

}





// (INCOMING)
// Open a new socket/port for incomming connections, set to nonblocking if
// required.
//
// IN:  (int *) "port" number. If pre-set to 0 it will pick first available
//      port and assign
// OUT: (int) new file-descriptor / SOCKET, or -1 for error. (Slight WIN32
//       override)
// OUT: (int *) "port" filled with local port number (host-order)
//
int sockets_make(int *port, unsigned long interfaceX, int exclusive)
{
  int option_arg, new_socket;    // temporary argument int, and holder for
                                 // new socket
  int result;                    // temporary holder for return code testing
  struct sockaddr_in local_sin;  // temporary socket Internet address holder

	memset(&local_sin, 0, sizeof(struct sockaddr_in));

  // Request a new TCP socket in the Internet address family.
  new_socket = socket( AF_INET, SOCK_STREAM, 0 );

  // Verify it was successful
#ifdef WIN32
  if (new_socket == INVALID_SOCKET)
#else
  if (new_socket < 0 )
#endif
    {
      // Insert non-OS specific error handling here.
      perror("socket(): ");
      return -1;
    }



  // We set our new socket to be reusable, otherwise you end up with too
  // many dead ports.
  option_arg = 1;
  // Unfortunately, most OS's use void * for the argument, except for
  // windows, where it is char *

#if defined (WIN32) && defined (SO_EXCLUSIVEADDRUSE)
  if (exclusive) {

	  setsockopt(new_socket, SOL_SOCKET, SO_EXCLUSIVEADDRUSE,
				 (void *)&option_arg,
				 sizeof(option_arg));

  } else {
	  setsockopt(new_socket, SOL_SOCKET, SO_REUSEADDR, (void *)&option_arg,
				 sizeof(option_arg));
  }

#else // WIN32

  setsockopt(new_socket, SOL_SOCKET, SO_REUSEADDR, (void *)&option_arg,
			 sizeof(option_arg));

#endif




  // If we want non-blocking mode, set it now
#ifdef NONBLOCKING_SOCKETS

  new_socket = sockets_setnonblocking(new_socket);
  if (new_socket < 0) return -1;

#endif


  // Setup the socket address structure
  local_sin.sin_family      = AF_INET;

  if (!interfaceX)
    local_sin.sin_addr.s_addr = htonl(INADDR_ANY);
  else
    local_sin.sin_addr.s_addr = htonl(interfaceX);

  //  printf("%08X %08X\n", interface, htonl(interface));

  local_sin.sin_port        = htons((unsigned short)*port); /* Any port */


  //	printf("bind()ing %s\n", ul2a(l_sin.sin_addr.s_addr));


  // Lets bind this new socket to a port
  result = bind( new_socket, (struct sockaddr *) &local_sin,
		 sizeof(local_sin));

#ifdef WIN32

  if ( result == SOCKET_ERROR )
    {

      // Windows don't set the return code in "some" of their network calls.
      // This is a bit fudgy, but the remaining code stays portable
      errno = WSAGetLastError();

      perror("bind: ");
      closesocket(new_socket);

      return -1;
    }


#else

  if ( result )
    {

      perror("bind: ");
      close(new_socket);

      return -1;
    }

#endif


  // If the port was 0 and we picked a free one, now it's time to check
  // which port we were assigned.
  option_arg = sizeof(struct sockaddr_in);
  getsockname(new_socket, (struct sockaddr *) &local_sin, &option_arg);


  // Get the socket listening for new connections, the queue parameter is
  // obsolete but is required for nostalgic reasons.
  listen(new_socket, 5);

  // Assigned it to the passed variable
  *port = ntohs(local_sin.sin_port);

  // Finally, return the new and shining socket
  return new_socket;
}






// (INCOMING)
// accept a new connection on a socket.
//
// IN: socket where the new connection is on
// IN: <optional> (ULONG *) remote host's address (host-order)
// IN: <optional> (UINT  *) remote host's port    (host-order)
// OUT: the new socket, or -1 for error
//
int sockets_accept(int sock, unsigned long *remhost, int *remport)
{
  int new_socket;		// the new socket
  struct sockaddr_in from;	// temporary address holder
  int len = sizeof(from);	// temporary integer variable of address size


  // Call accept
  if (remhost)
    new_socket = accept(sock, (struct sockaddr *)&from, &len);
  else
    new_socket = accept(sock, NULL, NULL);


#ifdef WIN32
  if (new_socket == INVALID_SOCKET) return -1;
#else
  if (new_socket < 0) return -1;
#endif


  // bcopy is non-standard so we change to ansi memcpy
  // bcopy(&from.sin_addr.s_addr, remhost, sizeof(unsigned long));

  // copy over the actual IP, this *will* break with IPng.

  // FIXME!! We use Network byte order internally to LION.
  if (remhost)
    {
      memcpy((void *)remhost, (void *)(&from.sin_addr.s_addr),
	     sizeof(unsigned long));
      *remhost = ntohl(*remhost);

      // and port, in host-order
      *remport = ntohs(from.sin_port);
    }


#if !defined ( __Linux__ ) && defined ( buffer_size )

  /*
   *
   * This is by no means crucial for the ftpd to work, but will make it run
   * smoother. It is part of POSIX but lame OS, like Linux, decide only to
   * implement POSIX half-heartedly. But as always, the popular OS' are never
   * the good OS'. (See ms-dos, Win95, Linux etc)
   *
   */


  // Set low-water mark, which is still not implemented by Linux.
  len = buffer_size;
  len = setsockopt(new_socket, SOL_SOCKET, SO_SNDLOWAT, &len, sizeof(len));

  printf("[sockets] set LOWAT to %d:%d\n", buffer_size, len);

#endif


  new_socket = sockets_setnonblocking(new_socket);

  // return new socket just connected
  return new_socket;
}




// (OUTGOING)
// Establish a new connection to a remote host
//
// IN: (ULONG) address of remote host
// IN: (UINT)  port of remote host
// OUT: the new socket, or -1 for error
//
int sockets_connect(unsigned long remote_host, unsigned int remote_port,
					unsigned long iface, unsigned int iport)
{
  struct sockaddr_in local_sin;  // temporary address holder
  int new_socket;                // the new socket to be returned
  int result;
#if !defined (__Linux__)
  int option_arg;
#endif

  // Allocate new socket

  // This seem to apply for ALL unix - why?

  //#ifdef __NetBSD__
  // seems pth returns 0 sometimes, which isn't very nice, since
  // multiple connections would all get 0, so we keep calling until it
  // is good.
  while (1) {
	  //#endif

	  new_socket = socket( AF_INET, SOCK_STREAM, 0 );

	  //#ifdef __NetBSD__
	  if (new_socket) break;
  }
  //#endif



#ifdef WIN32
  if (new_socket == INVALID_SOCKET)
#else
  if (new_socket < 0)
#endif
      {
		  perror("socket_connect(socket())");
		  return -1;
      }



  // Fill in the members of the address structure and byte swap
  memset(&local_sin, 0, sizeof(local_sin));

  // Set up neccessary variables.
  local_sin.sin_family = AF_INET;

  // Now if optional iface or iport was given, we need to bind() first

  if (iface || iport) {

#ifdef DEBUG
	  printf("[sockets] iface or iport set: %08lx:%u\n", iface, iport);
#endif


	  // If we bound a local port, lets release it too.
	  option_arg = 1;

	  setsockopt(new_socket, SOL_SOCKET, SO_REUSEADDR, (void *)&option_arg,
				 sizeof(option_arg));






	  if (!iface)
		  local_sin.sin_addr.s_addr = htonl(INADDR_ANY);
	  else
		  local_sin.sin_addr.s_addr = htonl(iface);

	  local_sin.sin_port = ntohs((unsigned short)iport);



	  result = bind( new_socket, (struct sockaddr *) &local_sin,
					 sizeof(local_sin));

#ifdef WIN32

	  if ( result == SOCKET_ERROR ) {

		  // Windows don't set the return code in "some"of their network calls.
		  // This is a bit fudgy, but the remaining code stays portable
		  errno = WSAGetLastError();

		  perror("bind: ");
		  closesocket(new_socket);

		  return -1;
	  }


#else

	  if ( result ) {

		  perror("bind: ");
		  close(new_socket);

		  return -1;
	  }

#endif


#ifdef DEBUG
	  printf("[sockets] bound successfully.\n");
#endif

  }


  // Set up the struct for connecting...
  local_sin.sin_port = htons((unsigned short)remote_port);

  // Byte swap host
  remote_host = htonl(remote_host);

  // copy over the address
  memcpy(&local_sin.sin_addr, &remote_host, sizeof(remote_host)) ;


#if !defined ( __Linux__ )
  // Set low water mark on all but Linux
  option_arg = buffer_size;
  setsockopt(new_socket, SOL_SOCKET, SO_SNDLOWAT, &option_arg,
	     sizeof(option_arg));

#endif


  // Enable non-blocking mode if required
#ifdef NONBLOCKING_SOCKETS

  new_socket = sockets_setnonblocking(new_socket);

#endif // NONBLOCKING_SOCKETS

  if (new_socket < 0) return -1;

  errno = 0;


  // Attempt to connect to remote host
  result = connect(new_socket,(struct sockaddr *)&local_sin,
		   sizeof(local_sin));


  // Check the error, some OS's return an error to say it is in progress,
  // others
  // return just success, so we have to check for both eventualities.
#ifdef WIN32
  errno = WSAGetLastError();

  if( (result == SOCKET_ERROR) && ( errno != WSAEINPROGRESS)  && (errno != WSAEWOULDBLOCK))
    {
      /* Doh, failed. */
      closesocket(new_socket);
      return -1;
    }

#else

  /*
   * We have a situation where connect() returns EADDRINUSE at "random" times,
   * and no ammount of re-trying connect() will work. This is definitely happening
   * on a NetBSD-4.0.1-i386 system. It would appear that closing the socket, and
   * re-doing the bind() and connect() will work second time around.
   *
   */
#ifdef EADDRINUSE
  if (result == -1 && errno == EADDRINUSE) {
      // Calling ourselves could result in an infinite loop though...
#ifdef DEBUG
      if (!recursion_reentry) {

          printf("sockets_connect(%d) returned EADDRINUSE, will re-attempt connect() call.\n",
                 new_socket);

          close(new_sock);
          errno = 0;

          recursion_reentry = 1;
          new_socket = sockets_connect(remote_host, remote_port,
                                       iface, iport);
          recursion_reentry = 0;

          if (errno == EADDRINUSE)
              printf("sockets_connect(): socket is still broken, returning error.\n");
          else
              printf("sockets_connect(): re-attempt fixed the socket.\n");
#endif

  }
#endif // EADDRINUSE



  // For some reason, on Solaris, errno is 0, but perror reports
  // "Operation now in progress". So uhmm, how should I test for it?
#ifdef __sun__
  if ((errno == 0) && (result == -1))
    return new_socket;
#endif

 if( result && ( errno != EINPROGRESS ) && ( errno != EWOULDBLOCK))
    {
      /* Doh, failed. */
      close(new_socket);
      return -1;
    }

#endif

  // Return the new socket. If we are in nonblocking, this is NOT connected
  // yet and may
  // still fail
  return new_socket;

}


int sockets_connect_udp(int fd, unsigned long remote_host,
                        unsigned int remote_port)
{
    int ret;
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(struct sockaddr_in));
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = htonl(remote_host);
    addr.sin_port        = htons(remote_port);

    ret = connect(fd,
                  (struct sockaddr *) &addr,
                  sizeof(addr));
    return ret;
}



// Attempt to retrieve more data from a socket
//
// IN: socket, buffer area, max read (a-la read(). )
// OUT: number of bytes written to buffer, 0 closed gracefully, -1 lost.
//
int sockets_read(int fd, unsigned char *buffer, unsigned int len)
{
  int result;
  // We use read() for Unix platforms as it's the most portable
  // even tho most systems also have recv().  Win32 can't call
  // read() and amusing things happen when you try.

#ifdef WIN32
  result = recv(fd, buffer, len, 0);
#else
  result = read(fd, buffer, len);
#endif

#ifdef DEBUG_VERBOSE
  printf("Read %d ret %d\n", fd, len);
#endif

  return result;
}




// Attempt to transmit more data to a socket
//
// IN: socket, buffer area, number of bytes to send.
// OUT: number of bytes sent, 0 closed gracefully, -1 lost.
//
int sockets_write(int fd, unsigned char *buffer, unsigned int len)
{
  int result;

#ifdef WIN32
  result = send(fd, buffer, len, 0);
#else
  result = write(fd, buffer, len);
#endif


  return result;

}




//
// Standard formattable printing routine. Testing only, no buffer checks!
//
// IN:  socket to transmit to
// IN:  format and arguments a'la printf
// OUT: bytes written or -1 for error, or 0 for connection gracefully closed.
//
int
#if HAVE_STDARG_H
sockets_printf(int fd, char const *fmt, ...)
#else
sockets_printf(fd, fmt, va_alist)
     int fd;
     char const *fmt;
     va_dcl
#endif
{
	va_list ap;
	char msg[1024];
	int result;

	VA_START (ap, fmt);

	// Buffer overflow! No vsnprintf on many systems :(
	vsnprintf(msg, sizeof(msg), fmt, ap);
	va_end(ap);

#ifdef WIN32

	result = send(fd, msg, strlen(msg), 0);

#else

	result = write(fd, msg, strlen(msg));

#endif

	return result;

}


unsigned long sockets_gethost(char *host)
{
  unsigned long result;
  struct hostent *h;


  // Is string in dot notation?
  result = inet_addr( host );

  if (result == INADDR_NONE) {

    // Nope, try to look it up as a host name
    h = gethostbyname(host);

    if (!h) {
      // Failed to look up hostname
      return 0;  // 0 is a valid IP :(
    }

    memcpy(&result, h->h_addr, sizeof(result));

  }

  result = ntohl(result);

  return result;

}




char *ul2a(unsigned long q)
{
  THREAD_SAFE static char host[64];

  //q = ntohl(q);  // Bring it to host order

  sprintf(host, "%u.%u.%u.%u",
          ((unsigned char *)&q)[0], /* Assume network order */
          ((unsigned char *)&q)[1],
          ((unsigned char *)&q)[2],
          ((unsigned char *)&q)[3]);

  return host;
}




//
// Takes a PASV reply string, and fetches out the details to place
// in the variables passed to it.
//
int sockets_pasv(char *line, unsigned long *host, unsigned short int *port)
{
  unsigned char *pp;
  unsigned int IPA[4], PORTA[2];

  pp = (unsigned char *)strchr(line, '(');

  if (!pp) {
	  // If we don't have a '(' assume it's just the numbered string.
	  pp = (unsigned char *)line;
  } else pp++;

  if (sscanf((char *)&pp[0],
			 "%u,%u,%u,%u,%u,%u", &IPA[0], &IPA[1], &IPA[2], &IPA[3],
             &PORTA[0], &PORTA[1]) != 6) {
    return 0;
  }

  pp = (unsigned char *)(host);
  pp[0] = (unsigned char) IPA[0];
  pp[1] = (unsigned char) IPA[1];
  pp[2] = (unsigned char) IPA[2];
  pp[3] = (unsigned char) IPA[3];
  pp = (unsigned char *)(port);
  pp[0] = (unsigned char) PORTA[0];
  pp[1] = (unsigned char) PORTA[1];


  //*host = ntohl(*host);

  //  printf("asd %08X\n", *host);

  //*port = ntohs(*port);

  return 1;
}



//
// Take a host and port pair, and produce a PORT string.
//

char *sockets_port(unsigned long host, unsigned short int port)
{
  unsigned char *pp;
  unsigned int IPA[4], PORTA[2];
  THREAD_SAFE static char result[40]; // Maximum string is 31.

  pp = (unsigned char *)&(host);
  IPA[0] = (unsigned char) pp[0];
  IPA[1] = (unsigned char) pp[1];
  IPA[2] = (unsigned char) pp[2];
  IPA[3] = (unsigned char) pp[3];

  //port = ntohs(port);

  pp = (unsigned char *)&(port);
  PORTA[0] = pp[0];
  PORTA[1] = pp[1];

  sprintf(result, "%u,%u,%u,%u,%u,%u",
             IPA[0], IPA[1], IPA[2], IPA[3], PORTA[0], PORTA[1]);

  return result;

}


unsigned long sockets_getsockname(int fd, int *port)
{
  struct sockaddr_in par;
  int len;
  unsigned long result;

  len = sizeof(par);

  if (getsockname(fd, (struct sockaddr *)&par, &len) < 0) {

    //bcopy(&localhost, &(t->port_host), sizeof(localhost));
    return INADDR_NONE;

    }

  memcpy(&result, &par.sin_addr, sizeof(result));

  if (port)
	  *port = (int) ntohs( par.sin_port );

  return result;

}


unsigned long sockets_getpeername(int fd, int *port)
{
  struct sockaddr_in par;
  int len;
  unsigned long result;

  len = sizeof(par);

  if (getpeername(fd, (struct sockaddr *)&par, &len) < 0) {

	  //bcopy(&localhost, &(t->port_host), sizeof(localhost));
	  return INADDR_NONE;

  }

  memcpy(&result, &par.sin_addr, sizeof(result));

  if (port)
	  *port = (int) ntohs( par.sin_port );

  return result;

}



//
// Windows don't come with socketpair, and NetBSD's socketpair is
// broken. (Including NetBSD-2.0)
//
#if !defined (WIN32) && !defined (__NetBSD__)

int sockets_socketpair(int *fd)
{
	int ret;

	ret = socketpair(AF_UNIX, SOCK_STREAM, 0, fd);

	if (ret) return ret;

	ret = 1;


	fd[0] = sockets_setnonblocking(fd[0]);
	fd[1] = sockets_setnonblocking(fd[1]);

	if ((fd[0] < 0) || (fd[1] < 0)) return -1;

	return 0;
}

#else

int sockets_socketpair(int *sv)
{
	int temp, s1, s2, result;
    struct sockaddr_in saddr;
    int nameLen;
    unsigned long option_arg = 1;

	nameLen = sizeof(saddr);

    /* ignore address family for now; just stay with AF_INET */
	temp = socket(AF_INET, SOCK_STREAM, 0);
#ifdef WIN32
	if (temp == INVALID_SOCKET) return -1;
#else
	if (temp < 0 ) return -1;
#endif


	setsockopt(temp, SOL_SOCKET, SO_REUSEADDR, (void *)&option_arg,
			   sizeof(option_arg));


    /* We *SHOULD* choose the correct sockaddr structure based
	   on the address family requested... */
	memset(&saddr, 0, sizeof(saddr));

    saddr.sin_family = AF_INET;
    saddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    saddr.sin_port = 0; // give me a port


	result = bind(temp, (struct sockaddr *)&saddr, nameLen);
#ifdef WIN32
	if ( result == SOCKET_ERROR )  {
		errno = WSAGetLastError();
		closesocket(temp);
		return -2;
    }
#else
	if ( result ) {
		perror("bind: ");
		close(temp);
		return -2;
    }
#endif

	// Don't care about error here, the connect will fail instead
	listen(temp, 1);


	// Fetch out the port that was given to us.
    nameLen = sizeof(struct sockaddr_in);


    result = getsockname(temp, (struct sockaddr *)&saddr, &nameLen);

#ifdef WIN32
	if (result == INVALID_SOCKET) {
		closesocket(temp);
		return -4; /* error case */
    }
#else
  if ( result ) {
      //perror("getsockname: ");
      close(temp);
      return -4;
    }
#endif



	s1 = socket(AF_INET, SOCK_STREAM, 0);
#ifdef WIN32
	if (s1 == INVALID_SOCKET) {
		closesocket(temp);
		return -5;
	}
#else
	if (s1 < 0 ) {
		close(temp);
		return -5;
	}
#endif

    nameLen = sizeof(struct sockaddr_in);

    result = connect(s1, (struct sockaddr *)&saddr, nameLen);

#ifdef WIN32
	if (result == INVALID_SOCKET) {
		closesocket(temp);
		closesocket(s1);
		return -6; /* error case */
    }
#else
	if ( result ) {
      //perror("getsockname: ");
		close(temp);
		close(s1);
		return -6;
	}
#endif

	s2 = accept(temp, NULL, NULL);

#ifdef WIN32
	closesocket(temp);

	if (s2 == INVALID_SOCKET) {
		closesocket(s1);
		return -7;
	}
#else
	close(temp);

	if (s2 < 0 ) {
		close(s1);
		return -7;
	}
#endif




    sv[0] = s1; sv[1] = s2;

	// Make them nonblocking now.
	temp = 1;

	sv[0] = sockets_setnonblocking(sv[0]);
	sv[1] = sockets_setnonblocking(sv[1]);

	if ((sv[0] < 0) || (sv[1] < 0)) return -8;

	return 0;  /* normal case */
}



#endif



int sockets_peek(int fd, unsigned char *buffer, unsigned int len)
{

	return recv(fd, buffer, len, MSG_PEEK);

}






int sockets_udp(int *port, unsigned long iface)
{
	int option_arg = 1;
	int new_socket = -1;
	int result = 0;                    // temporary holder for return code testing
	struct sockaddr_in local_sin;  // temporary socket Internet address holder

	if (!port) return -1;

	memset(&local_sin, 0, sizeof(struct sockaddr_in));


	new_socket = socket( AF_INET, SOCK_DGRAM, 0 );

	// Verify it was successful
#ifdef WIN32
	if (new_socket == INVALID_SOCKET) {
		// Insert non-OS specific error handling here.
		perror("socket(): ");
		return -1;
	}
#else
	if (new_socket < 0 ) {
		// Insert non-OS specific error handling here.
		perror("socket(): ");
		return -1;
	}
#endif




	option_arg = 1;
	// Unfortunately, most OS's use void * for the argument, except for
	// windows, where it is char *

	result = setsockopt(new_socket, SOL_SOCKET, SO_REUSEADDR, (void *)&option_arg,
						sizeof(option_arg));
#ifdef SO_REUSEPORT
	option_arg = 1;
	result += setsockopt(new_socket, SOL_SOCKET, SO_REUSEPORT, (void *)&option_arg,
						 sizeof(option_arg));
#endif

	//printf("[sockets] setsockopt(REUSEADDR,REUSEPORT) = %d\n", result);

	// Setup the socket address structure
	local_sin.sin_family      = AF_INET;

	if (!iface)
		local_sin.sin_addr.s_addr = htonl(INADDR_ANY);
	else
		local_sin.sin_addr.s_addr = htonl(iface);

	//  printf("%08X %08X\n", interface, htonl(interface));

	local_sin.sin_port        = htons((unsigned short)*port); /* Any port */


	//	printf("bind()ing %s\n", ul2a(l_sin.sin_addr.s_addr));


	// Lets bind this new socket to a port
	result = bind( new_socket, (struct sockaddr *) &local_sin,
				   sizeof(local_sin));

#ifdef WIN32

	if ( result == SOCKET_ERROR )
		{

		 // Windows don't set the return code in "some" of their network calls.
			// This is a bit fudgy, but the remaining code stays portable
			errno = WSAGetLastError();

			perror("bind: ");
			closesocket(new_socket);

			return -1;
		}


#else

	if ( result )
		{

			perror("bind: ");
			close(new_socket);

			return -1;
		}

#endif

	// If we want non-blocking mode, set it now
#ifdef NONBLOCKING_SOCKETS

	new_socket = sockets_setnonblocking(new_socket);

#endif


	// If the port was 0 and we picked a free one, now it's time to check
	// which port we were assigned.
	option_arg = sizeof(struct sockaddr_in);
	getsockname(new_socket, (struct sockaddr *) &local_sin, &option_arg);


	// Get the socket listening for new connections, the queue parameter is
	// obsolete but is required for nostalgic reasons.
	//listen(new_socket, 5);

	// Assigned it to the passed variable
	*port = ntohs(local_sin.sin_port);


	// Finally, return the new and shining socket
	return new_socket;
}



// Attempt to retrieve more data from a socket
//
// IN: socket, buffer area, max read (a-la read(). )
// OUT: number of bytes written to buffer, 0 closed gracefully, -1 lost.
// OUT: plus host and port assigned.
//
int sockets_recvfrom(int fd, unsigned char *buffer, unsigned int size,
					 unsigned long *host, unsigned int *port)
{
  int result;
  struct sockaddr_in from;	// temporary address holder
  int len = sizeof(from);	// temporary integer variable of address size


  result = recvfrom(fd, buffer, size, 0, (struct sockaddr *)&from, &len);


  if (host && port) {

	  // internally to lion, we use Network-Byte-Order
	  memcpy((void *)host, (void *)(&from.sin_addr.s_addr),
			 sizeof(unsigned long));
	  *port = (unsigned int) from.sin_port;

  }


#ifdef WIN32
  if (result < 0) {

	  len = WSAGetLastError();

#ifdef DEBUG
	  printf("[sockets] recvfrom: %d, %x\n", len, len);
#endif

	  if (len == 10054) return -2;
  }

  // Under WIN32, if we send a UDP packet to a non listening port, it comes back
  // -1 / 10054 ECONNRST Connection reset. Annoying, so we then close the udp socket
  // which may not be desirable. Lets ignore it?

#endif

#ifdef DEBUG_VERBOSE
  printf("[sockets] recvfrom %d -> %d\n",
	fd, result);
  perror("recvfrom");
#endif

  return result;
}



int sockets_sendto(int fd, unsigned char *buffer, unsigned int size,
				   unsigned long host, unsigned int port)
{
  int result;
  struct sockaddr_in to;	// temporary address holder
  int len = sizeof(to);	// temporary integer variable of address size

  memset(&to, 0, sizeof(to));

  to.sin_family      = AF_INET;

  memcpy((void *)(&to.sin_addr.s_addr), (void *)&host,
		 sizeof(unsigned long)); // length should be dest length!
  to.sin_port = (short int)port;

#ifdef DEBUG_VERBOSE
  printf("[socket] sendto: %08lX:%d\n", host, port);
  printf("         sendto: %s:%d\n",
		 inet_ntoa(to.sin_addr), ntohs(port));
#endif

  result = sendto(fd, buffer, size, 0, (struct sockaddr *)&to, len);

  return result;
}

  // With UDP we _know_ that anything attempted to be sent larger than
  // around 1500 bytes will fail. Should we set size = 1500 then?


