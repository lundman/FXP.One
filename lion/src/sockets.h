
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

// $Id: sockets.h,v 1.4 2009/08/10 03:44:49 lundman Exp $
// Lowest level socket manipulation routines.
// Jorgen Lundman October 10, 1999

#ifndef SOCKETS_H
#define SOCKETS_H


/* Defines   */

//#define DATA_BUFSZ 4096
//#define DATA_BUFSZ 4088
//#define DATA_BUFSZ 1400
//#define DATA_THRESHHOLD 4000    // Any packets over this get dumped


/* Variables */





/* Functions */

int  sockets_init(void);
void sockets_exit(void);

void sockets_close(int);
int  sockets_make(int *, unsigned long, int);
int  sockets_accept(int, unsigned long *, int *);
int  sockets_connect(unsigned long, unsigned int, unsigned long, unsigned int);

int  sockets_read(int fd, unsigned char *buffer, unsigned int len);
int  sockets_write(int fd, unsigned char *buffer, unsigned int len);

int  sockets_printf(int, char const *, ...);
unsigned long sockets_gethost(char *host);
char *ul2a(unsigned long);
#ifdef WIN32
int getdtablesize( void );
#endif

int sockets_pasv(char *, unsigned long *, unsigned short int *);
char *sockets_port(unsigned long , unsigned short int );
unsigned long sockets_getsockname(int, int *);
int sockets_socketpair(int *);
unsigned long sockets_getpeername(int, int *);
int sockets_peek(int fd, unsigned char *buffer, unsigned int len);

// udp
int sockets_udp(int *, unsigned long);
int sockets_recvfrom(int, unsigned char *, unsigned int,
					 unsigned long *, unsigned int *);
int sockets_sendto(int, unsigned char *, unsigned int,
				   unsigned long, unsigned int);
int sockets_setnonblocking(int);
int sockets_connect_udp(int, unsigned long, unsigned int );
#endif
