
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

// $Id: io.h,v 1.3 2008/12/05 01:50:33 lundman Exp $
// Core input output functions, the main select() call and misc connectivity
// Jorgen Lundman November 9th, 1999
// Copyright (c) 1999 HotGen Studios Ltd <www.hotgen.com>

#ifndef IO_H
#define IO_H


/* Includes */

#include <sys/types.h>
#include <time.h>

#ifdef WIN32
#define WINDOWS_LEAN_AND_MEAN
#include <winsock.h>
#else
#include <unistd.h>
#endif



/* Defines   */

#define  IO_TIMEOUT_USEC    0
#define  IO_TIMEOUT_SEC     30


#define IO_READFD  1
#define IO_WRITEFD 2
#define IO_BOTH    (IO_READFD | IO_WRITEFD)



/* Variables */
extern time_t io_global_time;




//extern THREAD_SAFE time_t io_global_time;
THREAD_SAFE extern int io_force_loop;


/* Functions */

int  io_add_to_fdset( int, int);
//int  io_loop(void);
void io_set_fdset( fd_set *fdset_read, fd_set *fdset_write );
void io_process_fdset( fd_set *fdset_read, fd_set *fdset_write );

void io_set_fdsetread( int );
void io_set_fdsetwrite( int );
int  io_isset_fdsetread( int );
int  io_isset_fdsetwrite( int );
int  io_near_full( void );

int  io_input           ( connection_t *);
int  io_getline         ( connection_t *, char **);

int io_set_fdset_sub    ( connection_t *, void *, void *);
int io_process_fdset_sub( connection_t *, void *, void *);
int io_purge_sub        ( connection_t *, void *, void *);
void io_pushread(void);
int io_passbinary(connection_t *);
void io_process_rate( void );

void io_release_all( connection_t * );
void lion_doclose(connection_t *node, int graceful);

#ifdef WIN32
int io_isset_fdset( connection_t *, int);
void io_set_fdset_win32 ( connection_t *, int);

#endif


#endif




