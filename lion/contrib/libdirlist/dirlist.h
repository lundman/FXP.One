
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

#ifndef DIRLIST_H_INCLUDED
#define DIRLIST_H_INCLUDED

#include "queue.h"



// These are mutually exclusive.
#define DIRLIST_LONG                1    // ls -l
#define DIRLIST_SHORT               2    // ls -1
#define DIRLIST_XML                 4    // ls -X

#define DIRLIST_FILE                8    // ls -T return filename
#define DIRLIST_PIPE               16    // ls -P return through pipe as input.

// modifiers
#define DIRLIST_SORT_NAME          32 // ls (always sorts by name)
#define DIRLIST_SORT_DATE          64 // ls -t 
#define DIRLIST_SORT_SIZE         128 // ls -s (most OSs)
#define DIRLIST_SORT_CASE         256 // ls -C case insensitive
#define DIRLIST_SORT_REVERSE      512 // ls -r
#define DIRLIST_SORT_DIRFIRST    1024 // ls -D Windows style directories first.

#define DIRLIST_SHOW_RECURSIVE   2048 // ls -R

#define DIRLIST_SHOW_DOT         4096 // ls -a
#define DIRLIST_USE_CRNL         8192 // \n vs \r\n

#define DIRLIST_SHOW_DIRSIZE    16384 // -W change dirs "size" fld to bytecount

#define DIRLIST_SHOW_GENRE      32768 // -G change group to %s/.genre instead.


#define DIRLIST_TYPE_FILE      1
#define DIRLIST_TYPE_DIRECTORY 2
 



struct dirlist_child_struct {
	lion_t *handle;
	
	int requests;  // requests sent down the pipe, decremented on reply.
	int unique;    // counter for unique id.

	dirlist_queue_t *queue_head;  // For fast pop off
	dirlist_queue_t *queue_tail;  // For fast add on

	struct dirlist_child_struct *next;  // Next child helper.
};

typedef struct dirlist_child_struct dirlist_child_t;








int  dirlist_init( int num_helpers );
void dirlist_free( void );
int  dirlist_list( lion_t *, char *path, char *pr, int flags, void *user_arg );
void dirlist_send( dirlist_child_t *node, dirlist_queue_t *que );
int  dirlist_a2f ( char *str );



// These are somewhat special and should be called before dirlist_init.
void dirlist_hide_file(char *);

void dirlist_set_uidlookup( char *(*function)(int uid));
void dirlist_set_gidlookup( char *(*function)(int gid));
void dirlist_no_recursion( char * );

void dirlist_cancel( lion_t *);



#endif
