
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
// This file contains the actual logic to list a directory.
//

#define _WIN32_WINNT 0x0400

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <direct.h>
#include <sys/stat.h>

#define _WINDOWS_MEAN_AND_LEAN
// Ensure that wsock.h is NOT loaded, we want wsock2.h
#define _WINSOCKAPI_
#include <windows.h>
#include <winsock2.h>


// Call the correct stat()
#define stat _stat64


#include "lion.h"
#include "misc.h"

#include "dirlist.h"
#include "dirlist_child.h"
#include "dirlist_child_process.h"
#include "dirlist_child_sort.h"


#define TEMPLATE "/tmp/.dirlist.XXXXXXXX"


__RCSID("$libdirlist: dirlist_child_process_win32.c,v 1.28 2003/04/23 08:32:03 lundman Exp $");



THREAD_SAFE static file_t **file_entries = NULL;
THREAD_SAFE static unsigned int num_entries = 0;
THREAD_SAFE static unsigned int allocated = 500; // *2 is first allocation.


//
// Only need one copy of these when we recurse, so we
// define them here to save on our stack.
//
THREAD_SAFE static unsigned long longest_user = 0;
THREAD_SAFE static unsigned long longest_group = 0;
THREAD_SAFE static lion64u_t     longest_size = 0;
THREAD_SAFE static WIN32_FIND_DATA stbf;
THREAD_SAFE static int recurse_flags;
THREAD_SAFE static int recurse_id;
THREAD_SAFE static char root_path[_MAX_PATH];
THREAD_SAFE static int  root_relative = 0;

THREAD_SAFE static unsigned int stats_deepest = 0;
THREAD_SAFE static unsigned int stats_maximum = 0;
THREAD_SAFE static unsigned int stats_fullest = 0;
THREAD_SAFE static unsigned int stats_starttime = 0;

THREAD_SAFE static char *glob   = NULL;
THREAD_SAFE static char *precat = NULL;



static int dirlist_child_process_recurse(lion_t *fd, FILE *ffd, 
										 int depth, char *path);



// Biggest int64 in ascii is..
// "18446744073709551615" or, rather, it needs space of
// 20 + 1
char *win32_uid2name(unsigned long uid)
{
	static char ret[] = "windows";
	
	return strdup(ret);

}

char *win32_gid2name(unsigned long gid)
{
	static char ret[] = "owner";

	return strdup(ret);

}




void add_entry(  char *name,
				 unsigned long user,
				 unsigned long group,
				 lion64u_t size,
				 time_t date,
				 int type,
				 char *perm )
{
	file_t *newf;
	file_t **tmp_ptr = NULL;


	if ( !file_entries ||
		 (num_entries >= allocated)) { // Grow the list.

		allocated *= 2;

		tmp_ptr = realloc(file_entries, allocated * sizeof(file_t *));

		if (!tmp_ptr) return;

#ifdef DEBUG
		printf("  [dirlist_child_process] grew list to %u entries\n", 
			   allocated);
#endif
		file_entries = tmp_ptr;

	}


	newf = (file_t *) malloc( sizeof(*newf));

	if (!newf)
		return;

	memset(newf, 0, sizeof( *newf ));


	//	printf("  adding entry %d to %s\n", num_entries, name);


	file_entries[ num_entries++ ] = newf;


	// Copy the easy stuff over...
	newf->type = type;
	newf->date = date;
	newf->size = size;

	// Then the optionals...
	if (name) 
		newf->name = strdup( name );
	if (perm) 
		newf->perm = strdup( perm );

	newf->user = win32_uid2name( user );
	// If DIRLIST_SHOW_GENRE (-G) was given, and this entry is
	// a directory, look for a .genre file.
	if ((recurse_flags & DIRLIST_SHOW_GENRE) && 
		(type == DIRLIST_TYPE_DIRECTORY)) {
		char *oldpath, genre[22], *g; // Max genre?
		FILE *f;
		
		// remember where we are.
		oldpath = getcwd(NULL, 0);
		
		// if we can go one lower...
		if (!chdir( name )) {
			
			// attempt to open the .genre file..
			if ((f = fopen(".genre", "r"))) {

				memset(genre, 0, sizeof(genre));
				fgets(genre, sizeof(genre)-1, f);

				// TODO!! Check that the genre string doesn't have /r/n etc
				// since that would mess things up. What about spaces?
				for (g = genre; *g; g++)
					if (isspace(*g)) *g = '_';
				
				SAFE_COPY(newf->group, genre);

				fclose(f);
			}
					
			// Move us back.
			chdir(oldpath);

		} // chdir
				
		SAFE_FREE(oldpath);
		
	} // if DIRLIST

	if (!newf->group)
		newf->group = win32_gid2name( group );

}


// Either of these two will work fine.

#if 0
#define int_size(I) snprintf(NULL, 0, "%lu", (I))
#else
int int_size( lion64u_t size )
{
	int ret;

	if (!size) return 1;

	for (ret = 0; size > 0; size /= 10) ret++;

	return ret;

}
#endif


char *misc_file_date(time_t ftime)
{
  static char date[13];
  int i, j;
  char *longstring;
  
  j = 0;

  longstring = ctime(&ftime);

	// For some reasons, Win32 return NULL sometimes
	if (!longstring) {
	  longstring = ctime(&lion_global_time);
	  if (!longstring)
		  longstring = "Sat Jan  8 12:23:02 JST 2005";
	}

  for (i = 4; i < 11; ++i)
    date[j++] = longstring[i];
  
#define SIXMONTHS       ((365 / 2) * 60 * 60 * 24)

  if (ftime + SIXMONTHS > time(NULL))
    for (i = 11; i < 16; ++i)
      date[j++] = longstring[i];
  else {
    date[j++] = ' ';
    for (i = 20; i < 24; ++i)
      date[j++] = longstring[i];
  }
  date[j++] = 0;

  return date;
}




void print_entry( FILE *fd, int flags, file_t *node, int longest_name,
				  int longest_group, int longest_size)
{
	//	printf("print_fd %d %s\n", flags, node->name);


	// Skip dot-entries ?
	if (!(flags & DIRLIST_SHOW_DOT) && 
		node->name &&
		(node->name[0] == '.'))
		return;

	// Also support Windows style hidden names: "$Directory$"
	if (!(flags & DIRLIST_SHOW_DOT) && 
		node->name &&
		(node->name[0] == '$') &&
		(node->name[ strlen(node->name) -1 ] == '$'))
		return;


	if ( !(flags & DIRLIST_XML )) {
		
		if ( flags & DIRLIST_SHORT ) {
			
// "dirlist_child_process.c"
			
			fprintf(fd, "%s%s%s", 
				precat,
					node->name ? node->name : "(null)",
					(flags & DIRLIST_USE_CRNL) ? "\r\n" : "\n");
			
		} else if ( flags & DIRLIST_LONG ) {
			
// "-rw-r--r--  1 lundman  wheel   1274 Feb 15 17:42 dirlist_child_process.c"
			
			fprintf(fd, 
					"%11.11s 1 %-*.*s %-*.*s %*I64i %12.12s %s%s%s", 
					node->perm ? node->perm : "(null)",
					longest_name,  longest_name, 
					node->user ? node->user : "(null)",
					longest_group, longest_group, 
					node->group ? node->group : "(null)",
					longest_size, node->size,
					misc_file_date( node->date ),
					precat,
					node->name ? node->name : "(null)",
					(flags & DIRLIST_USE_CRNL) ? "\r\n" : "\n" );
			
		} 

		return;

	}  // XML! 
			
			

	if ( flags & DIRLIST_SHORT ) {
//  <name="sites" />

		fprintf(fd, "  <name=\"%s%s\"/>%s",  
			precat,
				node->name ? node->name : "(null)",
				(flags & DIRLIST_USE_CRNL) ? "\r\n" : "\n");

		return ;

	}

	// We assume LONG unless SHORT is specified.


//  <dir:directory name="sites" lastModified="1019666489000"
//      date="24.04.02 18:41" size="118"/>

		// We should always have name, size and date. The rest
		// are (potentially) optional.
		// We split up this fprintf, but luckily, FILE * is buffered
		// so it don't flush to disk for every call, and should still
		// be relatively efficient.
		fprintf(fd, "  <dir:%s name=\"%s%s\" size=\"%I64i\" lastModified=\"%lu\"",
			precat,
				(node->type & DIRLIST_TYPE_FILE) ? "file" : "directory",
				node->name ? node->name : "(null)",
				node->size,
				(unsigned long)node->date);

		if (node->user)	 fprintf(fd, " user=\"%s\"", node->user);
		if (node->group) fprintf(fd, " group=\"%s\"", node->group);
		if (node->perm)  fprintf(fd, " permissions=\"%s\"", node->perm);

		fprintf(fd, " date=\"%s\"", misc_file_date(node->date));

		fprintf(fd, "/>%s", 
				(flags & DIRLIST_USE_CRNL) ? "\r\n" : "\n" );


}

void print_entry_node( lion_t *fd, int flags, file_t *node, int longest_name,
				  int longest_group, int longest_size)
{
	//	printf("print_node %d %s\n", flags, node->name);



	// Skip dot-entries ?
	if (!(flags & DIRLIST_SHOW_DOT) && 
		node->name &&
		(node->name[0] == '.'))
		return;

	// Also support Windows style hidden names: "$Directory$"
	if (!(flags & DIRLIST_SHOW_DOT) && 
		node->name &&
		(node->name[0] == '$') &&
		(node->name[ strlen(node->name) -1 ] == '$'))
		return;


	if ( !(flags & DIRLIST_XML )) {
		
		if ( flags & DIRLIST_SHORT ) {
			
// "dirlist_child_process.c"
			
			lion_printf(fd, "%s%s%s", 
				precat,
					node->name ? node->name : "(null)",
					(flags & DIRLIST_USE_CRNL) ? "\r\n" : "\n");
			
		} else if ( flags & DIRLIST_LONG ) {
			
// "-rw-r--r--  1 lundman  wheel   1274 Feb 15 17:42 dirlist_child_process.c"
			
			lion_printf(fd, 
					"%11.11s 1 %-*.*s %-*.*s %*I64i %12.12s %s%s%s", 
					node->perm ? node->perm : "(null)",
					longest_name,  longest_name, 
					node->user ? node->user : "(null)",
					longest_group, longest_group, 
					node->group ? node->group : "(null)",
					longest_size, node->size,
					misc_file_date( node->date ),
					precat,
					node->name ? node->name : "(null)",
					(flags & DIRLIST_USE_CRNL) ? "\r\n" : "\n" );
			
		} 

		return;

	}  // XML! 
			
			

	if ( flags & DIRLIST_SHORT ) {
//  <name="sites" />

		lion_printf(fd, "  <name=\"%s%s\"/>%s",  
			precat,
				node->name ? node->name : "(null)",
				(flags & DIRLIST_USE_CRNL) ? "\r\n" : "\n");

		return ;

	}

	// We assume LONG unless SHORT is specified.


//  <dir:directory name="sites" lastModified="1019666489000"
//      date="24.04.02 18:41" size="118"/>

		// We should always have name, size and date. The rest
		// are (potentially) optional.
		// We split up this fprintf, but luckily, FILE * is buffered
		// so it don't flush to disk for every call, and should still
		// be relatively efficient.
	lion_printf(fd, "  <dir:%s name=\"%s%s\" size=\"%I64i\" lastModified=\"%lu\"",
				(node->type & DIRLIST_TYPE_FILE) ? "file" : "directory",
				precat,
				node->name ? node->name : "(null)",
				node->size,
				(unsigned long)node->date);

		if (node->user)	 lion_printf(fd, " user=\"%s\"", node->user);
		if (node->group) lion_printf(fd, " group=\"%s\"", node->group);
		if (node->perm)  lion_printf(fd, " permissions=\"%s\"", node->perm);

		lion_printf(fd, " date=\"%s\"", misc_file_date(node->date));

		lion_printf(fd, "/>%s", 
				(flags & DIRLIST_USE_CRNL) ? "\r\n" : "\n" );


}




#define SECS_BETWEEN_EPOCHS (lion64_t)11644473600
#define SECS_TO_100NS (lion64_t)10000000



time_t FILEFileTimeToUnixTime( FILETIME FileTime, long *nsec )
{
	lion64_t UnixTime;

    /* get the full win32 value, in 100ns */
    UnixTime = ((lion64_t)FileTime.dwHighDateTime << 32) + 
       FileTime.dwLowDateTime;

    /* convert to the Unix epoch */
     UnixTime -= (SECS_BETWEEN_EPOCHS * SECS_TO_100NS);
 
     if ( nsec )
     {
         /* get the number of 100ns, convert to ns */
         *nsec = (UnixTime % SECS_TO_100NS) * (lion64u_t)100;
     }
 
     UnixTime /= SECS_TO_100NS; /* now convert to seconds */
 
     if ( (time_t)UnixTime != UnixTime )
     {
         //WARN("Resulting value is too big for a time_t value\n");
     }
 
     return (time_t)UnixTime;
}
 



int win32_type(unsigned long mode)
{
	
	if (mode & FILE_ATTRIBUTE_DIRECTORY)
		return DIRLIST_TYPE_DIRECTORY;

	return DIRLIST_TYPE_FILE;
}


char *win32_strmode(unsigned long mode)
{
	THREAD_SAFE static char dir[12] = "drwxr-xr-x ";
	THREAD_SAFE static char fil[12] = "-rw-r--r-- ";


	if (mode & FILE_ATTRIBUTE_DIRECTORY) {
		dir[2] = (mode & FILE_ATTRIBUTE_READONLY) ? '-' : 'w';
		return strdup(dir);
	}

	fil[2] = (mode & FILE_ATTRIBUTE_READONLY) ? '-' : 'w';
	return strdup(fil);

}




char *sort_string( int flags )
{
	static char result[67]; // biggest possible.

	*result = 0;

	if (flags & DIRLIST_SORT_NAME)
		strcat(result, "name, ");
	if (flags & DIRLIST_SORT_DATE)
		strcat(result, "date, ");
	if (flags & DIRLIST_SORT_SIZE)
		strcat(result, "size, ");
	if (flags & DIRLIST_SORT_CASE)
		strcat(result, "case-insensitive, ");
	if (flags & DIRLIST_SORT_DIRFIRST)
		strcat(result, "directories-first, ");

	if (flags & DIRLIST_SHOW_RECURSIVE)
		strcat(result, "recursive, ");

	// If the string is not empty, kill the last ", ".
	if (*result)
		result[ strlen( result ) - 2 ] = 0;
	
	return result;

}


void xml_finish(lion_t *relay, FILE *ffd, int flags)
{
	// </dir:directory>

	if (ffd)
		fprintf(ffd, "</dir:directory>%s",
				(flags & DIRLIST_USE_CRNL) ? "\r\n" : "\n" );
	else
		lion_printf(relay, "</dir:directory>%s",
				(flags & DIRLIST_USE_CRNL) ? "\r\n" : "\n" );


}



void xml_dot(lion_t *relay, FILE *ffd, int flags, file_t *node,
			 int longest_user, int longest_group, int longest_size)
{
	//<dir:directory xmlns:dir="http://apache.org/cocoon/directory/2.0"
	//    name="stylesheets"
	//    lastModified="1019666489000"
	//    date="24.04.02 18:41"
	//    size="461"
	//    sort="name"
	//    reverse="false"
	//    requested="true">
	if (!node->dot_name) return;


	if (ffd) {
		fprintf(ffd, "<dir:directory name=\"%s\" size=\"%I64i\" lastModified=\"%lu\" full_path=\"%s%s\"%s",
				node->dot_name,
				node->size,
				(unsigned long)node->date,
				*precat ? precat : "./",
				&root_path[root_relative],
				(flags & DIRLIST_USE_CRNL) ? "\r\n" : "\n" );

		fprintf(ffd, "    date=\"%s\" sort=\"%s\" reverse=\"%s\"%s",
				misc_file_date( node->date ),
				sort_string( flags ),
				(flags & DIRLIST_SORT_REVERSE) ? "true" : "false",
				(flags & DIRLIST_USE_CRNL) ? "\r\n" : "\n" );

		fprintf(ffd, "    longest_user=\"%u\" longest_group=\"%u\" longest_size=\"%u\">%s",
				longest_user,
				longest_group,
				longest_size,
				(flags & DIRLIST_USE_CRNL) ? "\r\n" : "\n" );
		
	
	} else {


		lion_printf(relay, "<dir:directory name=\"%s\" size=\"%I64i\" lastModified=\"%lu\" full_path=\"%s%s\"%s",
					node->dot_name,
					node->size,
					(unsigned long)node->date,
					*precat ? precat : "./",
					&root_path[root_relative],
					(flags & DIRLIST_USE_CRNL) ? "\r\n" : "\n" );
		
		lion_printf(relay, "    date=\"%s\" sort=\"%s\" reverse=\"%s\"%s",
				misc_file_date( node->date ),
				sort_string( flags ),
				(flags & DIRLIST_SORT_REVERSE) ? "true" : "false",
				(flags & DIRLIST_USE_CRNL) ? "\r\n" : "\n" );

		lion_printf(relay, "    longest_user=\"%u\" longest_group=\"%u\" longest_size=\"%u\">%s",
				longest_user,
				longest_group,
				longest_size,
				(flags & DIRLIST_USE_CRNL) ? "\r\n" : "\n" );
		
	}

}


void dirlist_child_process_getdirsize(char *dirname, WIN32_FIND_DATA *stbf)
{
	// We use tally to add up the byte counts so it is the same type 
	// without us assuming a certain type or size of the type.
	HANDLE *dirp;
	WIN32_FIND_DATA stater;
	char *old_cwd = NULL;
	int out = 1;
	lion64u_t total;

	//printf("[dirsize] gather data for '%s'\n", dirname);

	total = 0;
	stbf->nFileSizeLow = 0;
	stbf->nFileSizeHigh = 0;

	// remember where we were before.
	old_cwd = getcwd(NULL, 0);


	// Open the dir if we can...

	if (chdir(dirname)) {
		SAFE_FREE(old_cwd);
		return;
	}

	// If anything fails here, we have to remember to go back up.

	if ((dirp = FindFirstFile("*", &stater)) == INVALID_HANDLE_VALUE) {
		chdir(old_cwd); // If this fails, we are in trouble :)
		SAFE_FREE(old_cwd);
		return;
	}

	//printf("[dirsize] chdir ok, opendir ok\n");


	// For all entries:
	for ( ; dirp; out = FindNextFile( dirp, &stater ) ) {

		if (!out) break;

		// Normally we check for "." and ".." but since we only
		// tally up regular-files, that test is faster than strcmp().
		if (!(stater.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {

			// If we will overflow, add an extra value to High.
			total += (lion64u_t) ((lion64u_t) stater.nFileSizeHigh << 32) + stater.nFileSizeLow;

		}

	}

	FindClose(dirp);

	// Return us up one level.
	//printf("[dirsize] going back up: total %qu\n", stbf->st_size);

	chdir(old_cwd); // If this fails, we are in trouble :)
	SAFE_FREE(old_cwd);

	stbf->nFileSizeLow = (DWORD) total & 0xffffffff;
	stbf->nFileSizeHigh = (DWORD)(total >> 32) & 0xffffffff;

}




void dirlist_child_process(lion_t *reply, int id, int flags, char *stub, char *path)
{
	char *tmpname, *r;
	unsigned long longest_user = 0;
	unsigned long longest_group = 0;
	unsigned long longest_size = 0;
	struct stat stbf2;
	int good;
	FILE *ffd = NULL;


	num_entries = 0;
	recurse_id = id;
	recurse_flags = flags;
	precat = stub;
	//tmpname = strdup( TEMPLATE );

#ifdef DEBUG
	printf("  [dirlist_child_process] doing work on '%s' flags %d - precat '%s'\n", path,
		   flags, precat);
#endif


	// We opened the directory. Did they want a file, or through the pipe?
	if (!(flags & DIRLIST_PIPE)) { // Not a pipe, open a tmp file

		for (good = 0; good < 5; good++) {
			tmpname = tempnam("c:/temp", "dirlist"); // get a random name
			if (tmpname) break;
		}


		if (tmpname) {
			ffd = fopen(tmpname, "wb");
		}

#ifdef DEBUG
		printf("  [dirlist_child_process] tmpname %s -> fopen %p:%d\n", 
			tmpname ? tmpname : "(null)",
			ffd, errno);
#endif

		if (!tmpname || !ffd) {

			SAFE_FREE(tmpname);

			lion_printf(reply, "%u %u %s\n", id, errno,
						strerror(errno));
			return;

		}

		// Temp file open!

	} else { // Use pipe instead

		//		fd = lion_fileno(reply); 
		ffd = NULL;

	}


    // Extra logic. We stat the entry given to us, and if this fails,
    // we reverse back to the final "/" and terminate. We then assume the
    // last is a "glab match" they want to do. Like, "/directory/po*.txt".
    // or, if what we stat is not-a-directory, it is also assumed.

    glob = NULL;

	
    r = strrchr(path, '/');
    if (r && !r[1]) { // if "r" is set (we found "/") and the next char is 0..
            *r = 0;
    }



    if (stat(path, &stbf2) ||
            !((stbf2.st_mode & S_IFMT) == S_IFDIR)) {

        r = strrchr(path, '/');
        if (r) {
                        
            *r = 0;
            glob = &r[1];

#ifdef DEBUG
            printf("[dirlist] assuming glob is '%s'.\n", glob);
#endif

        }


    }

    // Basically, if the string is "/file/path/dir/" with the trailing
    // "/" we will ensure to remove it. The recurse function below does not
    // need it, and if we want the hide_path and no_recurse tests to match
    // it is required. (Or user needs to change all their paths to have "//")

	// We have to maintain a full path along the way, if we are recursing
	// so we can print the full (but relative) path.
	strcpy(root_path, path);
	root_relative = strlen(root_path) + 1;
	
	stats_starttime = clock();
	

	// Windows hackery. If they specify a differnt drive, like i: to where we
	// are (Say c:) it will always fail. We then need to chdrive() first.



	// Fetch this directory (and recurse if wanted)
	// 16 bytes on the stack per tree depth.
	good = dirlist_child_process_recurse( reply, ffd, 0, path );


	root_relative = 0;

	// Not needed, just in case
	recurse_id = -1;
	recurse_flags = 0;

#ifdef DEBUG
	printf("  [dirlist_child_process] complete: duration %lu sec\n  max depth %u, total max entries %u, total max entries in any one dir %u\n",
		   (clock() - stats_starttime) / CLOCKS_PER_SEC,
		   stats_deepest,
		   stats_maximum,
		   stats_fullest);
#endif

	// If its a pipe, send stop
	if (good && (flags & DIRLIST_PIPE)) {
		lion_printf(reply, "%u stop\n", id);
	}


	if (!(flags & DIRLIST_PIPE)) { // Not a pipe, open a tmp file

		// Fetch the name of the tmp file
		fclose(ffd);
		
#ifdef DEBUG
		printf("   [dirlist_child_process] replying with tmpfile '%s'\n", 
			   tmpname);
#endif

		if (good)
			lion_printf(reply, "%u 0 %s\n", id, tmpname);

		SAFE_FREE(tmpname);

		return;

	}

}







static int dirlist_child_process_recurse(lion_t *reply, FILE *ffd, 
										 int depth, char *path)
{
	HANDLE *dirp;
	unsigned int i;
	unsigned int index;
	char *old_cwd = NULL;
	int out = 1;


	// Remember where we are in the list, so we can free it.
	index = num_entries; 
	old_cwd = getcwd(NULL, 0);


	if (depth > stats_deepest)
		stats_deepest = depth;


	// These are to be re-computed for every directory
	longest_size = longest_group = longest_user = 0;

#if 0
	printf("  %*.*s[dirlist_child_process_recurse] depth %u path '%s' cwd '%s' root '%s'\n",
		   depth, depth, "", depth, path, old_cwd, root_path);
#endif


	// If recursing, print the start directory line
	if (depth && !(recurse_flags & DIRLIST_XML)) {

		// "/tmp//ssh-026418aa:"
		if (ffd) fprintf(ffd,   "%s%s%s:%s", 
						 (recurse_flags & DIRLIST_USE_CRNL) ? "\r\n" : "\n",
						 *precat ? precat : "./",
						 &root_path[root_relative],
						 (recurse_flags & DIRLIST_USE_CRNL) ? "\r\n" : "\n" );
		else lion_printf(reply, " \n%s%s:\n", 
						 *precat ? precat : "./",
						 &root_path[root_relative]);

	}



	// Lets make sure its a frigging directory maybe?
	//printf("chdir says %d\n", chdir(path));
        // This works too good, we always assume we can list the top level
        // directory, since the application can check that before calling us. 
        // We just want to stop users doing recursive lists and see things lower
        // that perhaps they shouldn't see.
    if (depth && dirlist_child_norec_num &&
            dirlist_child_is_no_recursion(root_path)) {

            SAFE_FREE(old_cwd);
#if 0
            if (!depth) {
                lion_printf(reply, "%u 2 Permission Denied\n", recurse_id);
                    return 0;
            }
#endif
            if (ffd) fprintf(ffd,   "ls: %s: Recursion Denied%s", path,
                                             (recurse_flags & DIRLIST_USE_CRNL) ? "\r\n" : "\n" );
            else lion_printf(reply, "ls: %s: Recursion Denied\n", path);

            return 0;

    }


	if (chdir( path ) || 
		((dirp = FindFirstFile("*", &stbf)) == INVALID_HANDLE_VALUE)) {


		SAFE_FREE(old_cwd);


		// If its root, give real error back.
		if (!depth) {
			lion_printf(reply, "%u %u %s\n", recurse_id, errno,
						strerror(errno));
			return 0;
		}
		

		// in XML, do we pass up errors?
		if (recurse_flags & DIRLIST_XML)
			return 0;

		//		perror("chdir");
		//		printf("  dirp %p - getcwd '%s' \n", dirp, old_cwd);
			 

		// If recursing, give error on just the dir.
		// "ls: ssh-026418aa: Permission denied"
		if (ffd) fprintf(ffd,   "ls: %s: %s%s", path, strerror(errno),
						 (recurse_flags & DIRLIST_USE_CRNL) ? "\r\n" : "\n" );
		else lion_printf(reply, "ls: %s: %s\n", path, strerror(errno));

		return 0;

	}

	// List the directory, sending info to "fd".


	for ( ; dirp; out = FindNextFile( dirp, &stbf ) ) {

		if (!out) break;


        // glob set? no match? skip this.
        // This might be wrong, could be better to let it read in everything
        // then, in the printing of information, only print that which matches.
        if (glob && lfnmatch(glob, stbf.cFileName, 0))
            continue;



        // Check if we are to hide anything. Note we do this after the "."
        // code above as it relies on there being a ".". And yes, this means
        // you could never hide "." from the user (assuming they use SHOW_DOT
        // flag and you don't strip it).
        if (dirlist_child_hide_num && 
            dirlist_child_is_hide_file( stbf.cFileName ))
            continue;



		if ( ( recurse_flags & DIRLIST_SHORT) ) {


 
			add_entry( stbf.cFileName,
					   0,
					   0,
					   0,
					   0,
					   DIRLIST_TYPE_FILE,
					   NULL);


			// Hack! Hack! Hack!
			if (!strcmp(stbf.cFileName, ".") &&
				(num_entries > 0) &&
				!strcmp(file_entries[ num_entries-1 ]->name, "."))
				file_entries[ num_entries-1 ]->dot_name = 
					depth ? strdup(path) : strdup(".");



			continue;
		}


		// If we are doing SHORT mode, we don't need to stat()

		// To stat or to lstat.
		//
		// If you want to "hide" that some items are softlinks, and have
		// them appear to be a file, or directory, pointed to, you would use
		// stat(). This is often desirable, however, since we would recurse
		// it, it would be very easy to make a loop, and recursive listing
		// will go on forever until killed.
		//
		// If we want to show links, we call lstat. However, IF we then also
		// want to follow links that point to directories when recursing,
		// we need to then also call stat() to determin if the destination is
		// a directory. So slightly less efficient.
			
// -rw-r--r--  1 lundman  wheel   1274 Feb 15 17:42 dirlist_child_process.c

		// -W option to count up contents of the dir.
		// We modify contents of stbf, is that allowed?
		if ((recurse_flags & DIRLIST_SHOW_DIRSIZE) &&
			(stbf.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
			strcmp(stbf.cFileName, ".") &&
			strcmp(stbf.cFileName, ".."))
			dirlist_child_process_getdirsize(stbf.cFileName, &stbf);
		



			add_entry( stbf.cFileName, 
					   0,
					   0,
					   (lion64u_t) ((lion64u_t) stbf.nFileSizeHigh << 32) +
					   stbf.nFileSizeLow, 
					   FILEFileTimeToUnixTime(stbf.ftLastWriteTime, NULL),//date
					   win32_type(stbf.dwFileAttributes),      // type
					   win32_strmode(stbf.dwFileAttributes));  // file mode

			// Hack! Hack! Hack!
			if (!strcmp(stbf.cFileName, ".") &&
				(num_entries > 0) &&
				!strcmp(file_entries[ num_entries-1 ]->name, "."))
				file_entries[ num_entries-1 ]->dot_name =
					depth ? strdup(path) : strdup(".");

			
		

	} // readdir
		

	FindClose( dirp );


	if (num_entries > stats_maximum)
		stats_maximum = num_entries;

	if (num_entries - index > stats_fullest)
		stats_fullest = num_entries - index;





	// If sorting, sort now.
	//	printf("  [sort] calling sort from %p index %u (%p) for %d elements\n",
	//	   file_entries, index, &file_entries[ index ], num_entries - index);

#if 0
	for (i = index; i < num_entries; i++) {
		printf(">> %u '%p'\n", i,
			   file_entries[ i ]->name?file_entries[ i ]->name:"(null)");
	}
#endif

	dirlist_child_sort(recurse_flags, &file_entries[index], 
					   num_entries - index);
	//	printf("  ..sorted\n"); fflush(stdout);




	// If long format, work out the biggest length needed.
	if (!(recurse_flags & DIRLIST_SHORT)) {
		int len;
		lion64u_t len2, size = 0;

		for (i = index; i < num_entries; i++) {

			if ( file_entries[ i ]->user ) {

				len = strlen(file_entries[ i ]->user);
				if (len > longest_user)
					longest_user = len;

			}

			if ( file_entries[ i ]->group ) {

				len = strlen(file_entries[ i ]->group);
				if (len > longest_group)
					longest_group = len;

			}
 
			len2 = file_entries[ i ]->size;

			if (len2 > size)
				size = len2;

		} // for all files

		// Get the actual number of char's needed for size.
		longest_size = int_size( size );


	}

	// Give an extra space on user and group.
	longest_user++;
	longest_group++;

	//	printf("  longest are %ld %ld %ld\n", longest_user, longest_group, 
	//   longest_size);






	// If it is pipe, send the start, if we are at root. 
	if (!depth && (recurse_flags & DIRLIST_PIPE))
		lion_printf(reply, "%u start\n", recurse_id);




	
	// If it is XML, print header...
	if (!depth && (recurse_flags & DIRLIST_XML)) {


#define XML_HEAD "<?xml version=\"1.0\" standalone=\"yes\"?>%s" 


		if (ffd)
			fprintf(ffd, XML_HEAD, 
					(recurse_flags & DIRLIST_USE_CRNL) ? "\r\n" : "\n");
		else
			lion_printf(reply, XML_HEAD, 
						"\n");

		
	}



	//
	// Dump list to device..
	//
	for (i = index; i < num_entries; i++) {

		// If we are in XML, and LONG, we need to send a start of
		// directory if we are at "." (assuming it is first!)
		if ((DIRLIST_XML & recurse_flags) &&
			file_entries[ i ]->name &&
			!strcmp(file_entries[ i ]->name, ".")) {

			// If not the VERY first "." then send a dir finished first.
			//			if (i) xml_finish(reply, ffd, recurse_flags);

			// Send start of dir..
			xml_dot(reply, ffd, recurse_flags, file_entries[ i ],
					longest_user, longest_group, longest_size);

		}


		if (ffd)
			print_entry(ffd, recurse_flags, file_entries[ i ], 
						longest_user, longest_group, longest_size);
		else
			print_entry_node(reply, recurse_flags, file_entries[ i ], 
						longest_user, longest_group, longest_size);

	}

	if (recurse_flags & DIRLIST_XML)
		xml_finish(reply, ffd, recurse_flags);


	//
	// If we are to recurse, do so here.
	//
	if (recurse_flags & DIRLIST_SHOW_RECURSIVE) {

		for (i = index; i < num_entries; i++) {
			
			if ( file_entries[ i ]->type == DIRLIST_TYPE_DIRECTORY ) {
				int len;

				// Lets not recurse "." and "..", that's bad news.
				// also, if we are not listing dot entries, skip any
				// "." directory.
				if (!(recurse_flags & DIRLIST_SHOW_DOT) &&
					file_entries[ i ]->name[0] == '.') continue;

				if (!strcmp(".", file_entries[ i ]->name)) continue;
				if (!strcmp("..", file_entries[ i ]->name)) continue;


				// Before we recurse, build up the root_path
				len = strlen(root_path);
				
				// Check we have room to print, win32 dont have snprintf
				if ((len + strlen(file_entries[ i ]->name) + 1) < 
					_MAX_PATH) {

					strcat( &root_path[ len ],   "/" );
					strcat( &root_path[ len+1 ], file_entries[ i ]->name);

				}

				// Otherwise, recurse away!
				dirlist_child_process_recurse( reply, ffd, depth+1, 
											   file_entries[ i ]->name);

				// Restore the root_path
				root_path[ len ] = 0;

			}

		}

	}


	//	fflush(stdout);



	// Clear list, anything we've added goes out
	for (i = index; i < num_entries; i++) {
		
		SAFE_FREE( file_entries[ i ]->name );
		SAFE_FREE( file_entries[ i ]->user );
		SAFE_FREE( file_entries[ i ]->group );
		SAFE_FREE( file_entries[ i ]->perm );

		SAFE_FREE( file_entries[ i ]->dot_name );

		SAFE_FREE( file_entries[ i ] );

	}

	// Set the last element back to what it was before we were called.
	num_entries = index;


	// Since we "stepped" into this directory, we need to go back up one.
	if (old_cwd) {
		chdir(old_cwd);
		SAFE_FREE(old_cwd);
	}

	//	printf("  recurse return %d\n", depth);
	return 1;
}
