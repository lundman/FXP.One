
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

// $Id: pipe.h,v 1.3 2008/12/05 01:50:33 lundman Exp $
// Pipe header definitions
// Jorgen Lundman January 9th, 2003

#ifndef PIPE_H
#define PIPE_H


/* Defines   */

#define MAX_ARG 100

// To signal that a child's execv failed, we return this magic.
#define PIPE_MAGIC "\233G\007W"
//#define PIPE_MAGIC "P00N"

/* Variables */





/* Functions */

int      pipe_make_pipes(connection_t *, connection_t *);
void     pipe_set_signals(void);
void     pipe_clear_signals(void);
int      pipe_get_status( connection_t *);




#ifdef WIN32

// Functions in file_win32.c
int lion_filereader(connection_t *main_node, void *user_data, void *arg);
int lion_filewriter(connection_t *main_node, void *user_data, void *arg);
int pipe_child_starter(void *child_pipe);
int lion_stdinwriter(connection_t *main_node, void *user_data, void *arg);
int lion_stdinreader(connection_t *main_node, void *user_data, void *arg);
// Functions in pipe_win32.c
int      win32_socketpair(int af, int type, int protocol, int *sv);
void pipe_release_events(connection_t *parent_pipe, connection_t *child_out,
						 connection_t *child_in);


#endif

#endif






