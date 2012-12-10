
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

// $Id: lgroup.h,v 1.1 2006/09/01 06:03:42 lundman Exp $
// Lgroup rate limits.
// Jorgen Lundman April 20th, 2003

#ifndef LGROUP_H
#define LGROUP_H

#include <time.h>  // for time_t
#include "lion.h"

/* Includes */



// Could make a smaller number, but the OS usually deal in page sizes so it
// might not be worth doing that anyway.
#define LGROUP_INCREMENT 256

// Lgroup 0 is special, all nodes belong to it as it is the global
// cap for the whole lion library.
#define LGROUP_GLOBAL 0





/* Variables */


struct lgroup_struct {
	unsigned int used;
	unsigned int cps_in;
	unsigned int cps_out;
	int current_in;
	int current_out;
	time_t stamp_in;
	time_t stamp_out;
};

typedef struct lgroup_struct lgroup_t;




/* Functions */

void     lgroup_exit            ( void );

int      lgroup_new             ( void );
void     lgroup_free            ( int );

void     lgroup_set_rate_in     ( int, int ); 
void     lgroup_set_rate_out    ( int, int ); 


// lion_t calls
void     lgroup_add             ( connection_t *, int );
void     lgroup_remove          ( connection_t *, int );

// Only makes sense once we have multi-lgroup code
//int      lgroup_ismember        ( lion_t *, int );

int      lgroup_check_rate_in   ( connection_t *, int );
int      lgroup_check_rate_out  ( connection_t *, int );

void     lgroup_add_in          ( connection_t *, int );
void     lgroup_add_out         ( connection_t *, int );


#endif
