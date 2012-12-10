
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

// $Id: misc.h,v 1.3 2011/10/18 08:49:33 lundman Exp $
// Miscellaneous utility functions
// Jorgen Lundman November 25th, 1999
// Copyright (c) 1999 HotGen Studios Ltd <www.hotgen.com>

#ifndef MISC_H
#define MISC_H

#include <time.h>

// Defines

// Various safe #defines.
#define SAFE_FREE(X) { if ((X)) { free((X)); (X) = NULL; } }
#define SAFE_COPY(X, Y) { SAFE_FREE(X); if ((Y)) (X) = strdup((Y)); }
#define SAFE_UCOPY(X, Y) { SAFE_FREE(X); if ((Y)) (X) = (unsigned char *)strdup((Y)); }
#define SAFE_DUPE(X, Y) { if ((Y)) (X) = strdup((Y)); else (X) = NULL; }
#define SAFE_PRINT(X) (X) ? (X) : ""

#define SAFE_STRNCPY(X, Y, N) { strncpy( (X), (Y), (N) ); (X)[(N)-1] = 0; }



// Variables

extern char misc_digtoken_optchar;



// Functions

char *misc_digtoken(char **, char *);
char *mystrcpy(char *);
int   misc_hextobin(char *);
int   misc_bintohex(char *, int);
char *misc_strdup(char *);
int   mystrccmp(char *,char *);
time_t misc_getdate(char *);
char *misc_idletime(time_t );
char *misc_idletime2(time_t );
//char *misc_makepath(char *, char *);
char *misc_pasv2port(char *);
char *misc_strjoin(char *, char *);
char *misc_strjoin_plain(char *, char *);
int   misc_skipfile(char *, char *);
void  misc_stripslash(char *);
void  misc_undos_path(char *);
char *misc_strip(char *);
char *misc_url_encode(char *);
char *misc_url_decode(char *);


#endif

