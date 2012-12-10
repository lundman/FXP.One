
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

// $Id: file.c,v 1.6 2011/06/13 06:23:42 lundman Exp $
// File IO functions for the lion library
// Jorgen Lundman January 8th, 2003.


// Universal system headers

#ifndef WIN32
#if HAVE_CONFIG_H
#include <config.h>
#endif


#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <errno.h>
#include <signal.h>
#include <string.h>

#include <unistd.h>

#include <fcntl.h>   // for locking



#ifdef __sun__
extern const char *const sys_errlist[];
#endif



// Implementation specific headers

#include "connections.h"
#include "io.h"
#include "sockets.h"

#include "lion.h"




__RCSID("$LiON: lundman/lion/src/file.c,v 1.6 2011/06/13 06:23:42 lundman Exp $");



connection_t *lion_open(char *file, int flags, mode_t modes,
						int lion_flags, void *user_data)
{
	connection_t *newd;

	// This breaks FULLFILL, but then, passing NULL breaks lion API.
	if (!file)
		return NULL;

	newd = connections_new();

	newd->type = LION_TYPE_FILE;
	newd->user_data = user_data;

	if (lion_flags & LION_FLAG_TRACE)
		lion_enable_trace(newd);

	if (newd->trace)
		fprintf(trace_file, "%p: lion_open (%s)\n",newd, file);

	// Open the file, if issues, send event.

	if (lion_flags & LION_FLAG_EXCLUSIVE) {



		// They want exclusive, so lets step through the various ways this
		// can be done, in descending order of preference.
		// FYI, NetBSD has all of these, which made it easier to test.

#if defined O_EXLOCK
        // This works best on NetBSD for some reason.
		newd->socket = open( file, flags | O_EXLOCK | O_NONBLOCK, modes );

#elif defined F_SHARE
        {
            struct fshare locker;
            int ret;
            locker.f_access = F_WRACC;
            locker.f_deny = F_WRDNY;

            // Does the file already exist?
            newd->socket = open( file, O_RDWR, modes );
            if (newd->socket >= 0) {

                // id should just be something unique, and filedes are that
                locker.f_id = newd->socket;

                ret = fcntl( newd->socket, F_SHARE, &locker);
                if (ret == -1) {

                    // Did not get lock, fail the open
                    close(newd->socket);
                    newd->socket = -1;

                } else { // lock worked?

                    // Got lock, do we truncate? Append?
                    if (flags|O_TRUNC)
                        ftruncate(newd->socket, 0);
                    if (flags|O_APPEND)
                        lseek(newd->socket, SEEK_END, 0);

                }

            } else { // existing file, creating file

                newd->socket = open( file, flags|O_EXCL, modes );
                if (newd->socket >= 0) {

                    // id should just be something unique, and fd's are that
                    locker.f_id = newd->socket;
                    ret = fcntl( newd->socket, F_SHARE, &locker);

                    // Failed to lock..
                    if (ret == -1) {
                        close(newd->socket);
                        newd->socket = -1;
                    } // lock worked?

                } // created

            } // existing/created

        }

#elif defined LOCK_EX

        // this will not work

		newd->socket = open( file, flags, modes );

		if (newd->socket >= 0) {
			if (flock(newd->socket, LOCK_EX|LOCK_NB)) {
				close(newd->socket);
				newd->socket = -1;
			}
		}

#elif defined F_SETLK
        // FCNTL LOCKING WILL NOT WORK. fcntl locks do not stop
        // the same process (same PID) from locking against itself
        // which means it is meaningless in lion situations.
        // We could fork() to test locks... but ick..
		{
			struct flock locker;
            memset(&locker, 0, sizeof(locker));
			locker.l_start = 0;         // offset from whence
			locker.l_len   = 0;         // whole file
			locker.l_pid   = getpid();  // don't actually care
			locker.l_type  = F_WRLCK;
			locker.l_whence= SEEK_SET;  // start of file.

			newd->socket = open( file, flags, modes );

			if (newd->socket >= 0) {

				if (fcntl( newd->socket, F_SETLK, &locker) == -1) {

					close(newd->socket);
					newd->socket = -1;

				}
			}
		}

#elif defined LOCK_EX

		newd->socket = open( file, flags, modes );

		if (newd->socket >= 0) {
			if (flock(newd->socket, LOCK_EX|LOCK_NB)) {
				close(newd->socket);
				newd->socket = -1;
			}
		}

#elif defined F_TLOCK  // advisory only, but atleast we can stop within lion.

		newd->socket = open( file, flags, modes );

		if (newd->socket >= 0) {

			if ( lockf( newd->socket, F_TLOCK, 0) == -1) {

				close(newd->socket);
				newd->socket = -1;

			}
		}


#endif  // which locking type


		// Phew no exclusive, just open.

	} else {

		newd->socket = open( file, flags, modes );

	}

#ifdef DEBUG
	if (newd->socket < 0)
		perror("open");
	printf("net_open(%s): %d\n", file, newd->socket);
#endif

	if (newd->socket < 0) {

		if (lion_flags & LION_FLAG_FULFILL) {

			// Signal above layer of failure
			newd->status = ST_PENDING;

			io_force_loop = 1;
			return newd;
		}

		newd->status = ST_DISCONNECT;

		return NULL;
	}


	// File was opened ok, set nonblocking.
	// Technically not needed if the first locking method exist.

	// In Unix, we can just use the same method as sockets
	//fcntl(newd->socket,F_SETFL,(fcntl(newd->socket,F_GETFL)|O_NONBLOCK));
	newd->socket = sockets_setnonblocking(newd->socket);


	//	printf("[file] file %d is nonblocking %d\n", newd->socket,
	//	   fcntl(newd->socket, F_GETFL) & O_NONBLOCK);


	// We're done, tell application its ready.
	// Race condition here, we can signal app that it is CONNECTED but
	// since we've not returned the handle, the client will get rather
	// confused.

	newd->status = ST_PENDING;

	return newd;

}




#endif
