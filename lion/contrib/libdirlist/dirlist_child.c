
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

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#include "lion.h"

#include "dirlist_child.h"
#include "dirlist_child_userinput.h"

#include "lfnmatch.h" // thats OUR fnmatch, not the OS one.

#include "misc.h"


THREAD_SAFE int child_switch = 0;



char *(*dirlist_child_uid_lookup)(int) = NULL;
char *(*dirlist_child_gid_lookup)(int) = NULL;





int dirlist_child( lion_t *node, void *user_data, void *arg )
{

	// Set our handler!
	lion_set_handler( node, (lion_handler_t) dirlist_child_userinput );


	// Set to ignore SIGPIPE
#ifndef WIN32
	signal(SIGPIPE, SIG_IGN);
#endif


#ifdef DEBUG
	printf("  [dirlist_child] child %p started\n", node);
#endif

	while( !child_switch ) {

		//printf("  [dirlist_child] tick\n");

		lion_poll(0, 1);

	};

#ifdef DEBUG
	printf("  [dirlist_child] child %p exiting\n", node);
#endif

	return 0;

}



static char **dirlist_child_hide_files   = NULL;
// This one isn't static so we can do fast test against it in the loop.
int dirlist_child_hide_num       = 0;
static int dirlist_child_hide_allocated = 0;
#define HIDE_FILES_INCREMENT 10


void dirlist_child_hide_file(char *hideme)
{
	char **newd;


	// Grow list if required.
	if (dirlist_child_hide_num >= dirlist_child_hide_allocated) {

		newd = realloc(dirlist_child_hide_files,
					   dirlist_child_hide_allocated + HIDE_FILES_INCREMENT);

		if (!newd) {

#ifdef DEBUG
			printf("[dirlist] hide file: failed to grow memory array\n");
#endif
			return;

		}

		dirlist_child_hide_allocated += HIDE_FILES_INCREMENT;
		dirlist_child_hide_files = newd;


	}


	// Add to list.

	dirlist_child_hide_files[ dirlist_child_hide_num ] = strdup( hideme );
	dirlist_child_hide_num++;


}


int dirlist_child_is_hide_file( char *do_hide_me)
{
	int i;

	for (i = 0; i < dirlist_child_hide_num; i++) {

		if (!lfnmatch(dirlist_child_hide_files[ i ],
					 do_hide_me,
					 LFNM_CASEFOLD))
			return 1;

	}

	return 0;

}






static char **dirlist_child_norec_files   = NULL;
// This one isn't static so we can do fast test against it in the loop.
int dirlist_child_norec_num       = 0;
static int dirlist_child_norec_allocated = 0;
#define NOREC_FILES_INCREMENT 10


void dirlist_child_no_recursion(char *norec)
{
	char **newd;


	// Grow list if required.
	if (dirlist_child_norec_num >= dirlist_child_norec_allocated) {

		newd = realloc(dirlist_child_norec_files,
					   dirlist_child_norec_allocated + NOREC_FILES_INCREMENT);

		if (!newd) {

#ifdef DEBUG
			printf("[dirlist]  no recursion: failed to grow memory array\n");
#endif
			return;

		}

		dirlist_child_norec_allocated += NOREC_FILES_INCREMENT;
		dirlist_child_norec_files = newd;


	}


	// Add to list.

	dirlist_child_norec_files[ dirlist_child_norec_num ] = strdup( norec );
	dirlist_child_norec_num++;


}


int dirlist_child_is_no_recursion( char *do_recursion)
{
	int i;

	for (i = 0; i < dirlist_child_norec_num; i++) {

		if (!lfnmatch(dirlist_child_norec_files[ i ],
					 do_recursion,
					 LFNM_CASEFOLD))
			return 1;

	}

	return 0;

}





void dirlist_child_free(void)
{
	int i;

	dirlist_child_uid_lookup = NULL;
	dirlist_child_gid_lookup = NULL;


	for (i = 0; i < dirlist_child_hide_num; i++) {
		SAFE_FREE(dirlist_child_hide_files[ i ]);
	}
	SAFE_FREE(dirlist_child_hide_files);
	dirlist_child_hide_allocated = 0;
	dirlist_child_hide_num = 0;


	for (i = 0; i < dirlist_child_norec_num; i++) {
		SAFE_FREE(dirlist_child_norec_files[ i ]);
	}
	SAFE_FREE(dirlist_child_norec_files);
	dirlist_child_norec_allocated = 0;
	dirlist_child_norec_num = 0;

}

