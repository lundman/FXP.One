
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

//
// This file contains logic to do the sorting functions..
//

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>

#include "lion.h"
#include "misc.h"

#include "dirlist.h"
#include "dirlist_child_process.h"
#include "dirlist_child_sort.h"








static int sortby_name(const void *data1, const void *data2)
{
	return (strcmp(
				   (*(file_t **)data1)->name,
				   (*(file_t **)data2)->name)
			);
}

static int sortby_name_reverse(const void *data1, const void *data2)
{
	return -1*(strcmp(
					  (*(file_t **)data1)->name,
					  (*(file_t **)data2)->name)
			   );
}

static int sortby_name_case(const void *data1, const void *data2)
{
	return (strcasecmp(
					  (*(file_t **)data1)->name,
					  (*(file_t **)data2)->name)
			);
}

static int sortby_name_case_reverse(const void *data1, const void *data2)
{
	return -1*(strcasecmp(
						 (*(file_t **)data1)->name,
						 (*(file_t **)data2)->name)
			   );
}

static int sortby_size(const void *data1, const void *data2)
{ 
	if ( (*(file_t **)data1)->size >=
		 (*(file_t **)data2)->size)
		return -1;
	else 
		return 1;
}

static int sortby_size_reverse(const void *data1, const void *data2)
{
	if ( (*(file_t **)data1)->size >= 
		 (*(file_t **)data2)->size)
		return 1;
	else 
		return -1;
}

static int sortby_date(const void *data1, const void *data2)
{ 
	if ( (*(file_t **)data1)->date >=
		 (*(file_t **)data2)->date)
		return -1;
	else
		return 1;
}

static int sortby_date_reverse(const void *data1, const void *data2)
{ 
	if ( (*(file_t **)data1)->date >=
		 (*(file_t **)data2)->date)
		return 1;
	else 
		return -1;
}

static int sortby_dirfirst(const void *data1, const void *data2)
{ 
	// 1 is dir, 2 is dir  ->  0
	// 1 is dir, 2 is file -> -1
	// 1 is file, 2 is dir ->  1
	// 1 is file, 2 is file->  0
	if ( (*(file_t **)data1)->type ==
		 (*(file_t **)data2)->type)
		return 0;

	if ( (*(file_t **)data1)->type == DIRLIST_TYPE_DIRECTORY)
		return -1;
	
	return 1;
}






void dirlist_child_sort( int flags, file_t **list, int total)
{
	int (*sorter)(const void *, const void *);


	//
	// These are the sorting modifiers we know about.
	// DIRLIST_SORT_NAME DIRLIST_SORT_DATE DIRLIST_SORT_SIZE DIRLIST_SORT_CASE
	// DIRLIST_SORT_REVERSE DIRLIST_SORT_DIRFIRST
	// Note that the last one is a post processing thingy.
	
	int sort, i;


	sort = flags & (DIRLIST_SORT_NAME|DIRLIST_SORT_DATE|DIRLIST_SORT_SIZE|
					DIRLIST_SORT_CASE|DIRLIST_SORT_REVERSE);


	// Work out how to sort.
#define N DIRLIST_SORT_NAME
#define D DIRLIST_SORT_DATE
#define S DIRLIST_SORT_SIZE
#define C DIRLIST_SORT_CASE
#define R DIRLIST_SORT_REVERSE


	// If NAME is set, unset DATE, SIZE
	if (sort & DIRLIST_SORT_NAME)
		sort &= ~(DIRLIST_SORT_DATE | DIRLIST_SORT_SIZE);

	// if DATE is set, unset SIZE and CASE (NAME we already know isnt set)
	if (sort & DIRLIST_SORT_DATE)
		sort &= ~(DIRLIST_SORT_SIZE | DIRLIST_SORT_CASE);

	// is SIZE is set, unset CASE
	if (sort & DIRLIST_SORT_SIZE)
		sort &= ~(DIRLIST_SORT_CASE);


	sorter = NULL;

	switch(sort) {

	case N:
		sorter = sortby_name;
		break;
	case N|C:
		sorter = sortby_name_case;
		break;
	case N|R:
		sorter = sortby_name_reverse;
		break;
	case N|C|R:
		sorter = sortby_name_case_reverse;
		break;

	case D:
		sorter = sortby_date;
		break;
	case D|R:
		sorter = sortby_date_reverse;
		break;
	case S:
		sorter = sortby_size;
		break;
	case S|R:
		sorter = sortby_size_reverse;
		break;
	}

	if (!sorter) // No sorter set, just return
		return;


	//printf("  [dirlist_child_sort] sorting...\n");



	// If we want directories first, we need to sort it for that first.
	// then find the first non-directory entry, and call sort twice, 
	// once on directory, and once on files.

	if (flags & DIRLIST_SORT_DIRFIRST) {

		qsort((void *)list, total, sizeof(file_t *), sortby_dirfirst);

		// Loop and find first non-directory

		for ( i = 0; i < total; i++) {
			if (list[ i ]->type != DIRLIST_TYPE_DIRECTORY)
				break;
		}

		// If is the first element, or, the list, it is either
		// all files, or, all directories, and we can just sort it
		// once. Otherwise, sort twice.

		if ((i > 0) && (i < total)) {

#ifdef DEBUG
			printf("   [dirlist_child_sort] found split at %d\n", i);
#endif

			// Sort directories (they are first)
			qsort((void *)list, i, sizeof(file_t *), sorter);

			// Sort files
			qsort((void *)&list[ i ], total - i, sizeof(file_t *), sorter);

			return;

		} // two types


	} // DIRFIRST


	qsort((void *)list, total, sizeof(file_t *), sorter);







	// Lets not pollute the name space
#undef N
#undef D
#undef S
#undef C
#undef R


}


