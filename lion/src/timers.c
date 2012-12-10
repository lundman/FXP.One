
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

// $Id: timers.c,v 1.8 2009/08/13 13:00:40 lundman Exp $
// Wrapper calls for the lion library
// Jorgen Lundman January 21st, 2003


// NOTE: This should be fixed for efficiency. Even a circular array
// buffer for timers would be more efficient than the gross memcpy
// everytime.

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <string.h>

#include "lion.h"
#include "misc.h"


	// Do we just check things that HAVE expired, or, do we accept
	// anything with fuzz? What precision do people expect?
#ifndef TIMER_FUZZ_MAJOR
#define TIMER_FUZZ_MAJOR          0
#endif

#ifndef TIMER_FUZZ_MINOR
#define TIMER_FUZZ_MINOR       5000   // 1000000=1s, 500000=0.5s 50000=0.05s
#endif



__RCSID("$LiON: lundman/lion/src/timers.c,v 1.8 2009/08/13 13:00:40 lundman Exp $");


THREAD_SAFE static timers_t     **timers_head      = NULL;
THREAD_SAFE static unsigned int   timers_allocated = 0;
THREAD_SAFE static unsigned int   timers_inuse     = 0;
#define TIMERS_ALLOC_STEP 5


THREAD_SAFE static struct timeval fuzz;


#ifndef HAVE_GETTIMEOFDAY
#ifdef WIN32

#define WINDOWS_MEAN_AND_LEAN
#include <windows.h>

void gettimeofday(now)
  struct timeval *now;
{
      FILETIME win_time;

      GetSystemTimeAsFileTime(&win_time);
      /* dwLow is in 100-s nanoseconds, not microseconds */
      now->tv_usec = win_time.dwLowDateTime % 10000000 / 10;

      /* dwLow contains at most 429 least significant seconds, since 32 bits maxint is 4294967294 */
      win_time.dwLowDateTime /= 10000000;

      /* Make room for the seconds of dwLow in dwHigh */
      /* 32 bits of 1 = 4294967295. 4294967295 / 429 = 10011578 */
      win_time.dwHighDateTime %= 10011578;
      win_time.dwHighDateTime *= 429;

      /* And add them */
      now->tv_sec = win_time.dwHighDateTime + win_time.dwLowDateTime;
}
#endif




#endif





int timers_init(void)
{

	fuzz.tv_sec  = TIMER_FUZZ_MAJOR;
	fuzz.tv_usec = TIMER_FUZZ_MINOR;

	return 0;

}




void timers_free(void)
{
	int i;

	// Release all.
	for (i = 0; i < timers_inuse; i++) {
		timers_freenode(timers_head[i]);
		timers_head[i] = NULL;
	}

	SAFE_FREE(timers_head);
	timers_allocated = 0;
	timers_inuse = 0;

}


timers_t *timers_newnode(void)
{
	timers_t *result;


	result = (timers_t*) malloc(sizeof(*result));
	if (!result) return NULL;
	memset(result, 0, sizeof(*result));

	return result;
}


void timers_freenode(timers_t *node)
{

	SAFE_FREE(node);

}





int timers_add(timers_t *node)
{
	struct timeval thetime;
	time_t nnow;
	struct tm *ntm;
	timers_t **tmp;
	int i;

#ifdef DEBUG
	printf("[timers] adding timer %p\n", node);
#endif

	// Work out when, based on relative, or absolute time.
	if (node->flags & TIMERS_FLAG_ABSOLUTE) {

		time(&nnow);
		ntm = localtime(&nnow);
		if (!ntm) return -2;

		ntm->tm_hour = node->major;
		ntm->tm_min  = node->minor;
		ntm->tm_sec  = 0;

		nnow = mktime(ntm);

		node->when.tv_sec = nnow;
		node->when.tv_usec = 0;

	} else {

		// Get time now, add relative time.
		gettimeofday(&thetime, NULL);
		node->when.tv_sec  = thetime.tv_sec  + node->major;
		node->when.tv_usec = thetime.tv_usec + node->minor;
		while (node->when.tv_usec >= 1000000) {
			node->when.tv_sec++;
			node->when.tv_usec -= 1000000;
		}

	}

	// Check if we need to grow the list.
	if (!timers_allocated ||
		(timers_allocated <= timers_inuse)) {

		tmp = realloc(timers_head,
					  sizeof(timers_t) *  // coverity says this is wrong.
					  (timers_allocated + TIMERS_ALLOC_STEP));

		if (!tmp) return -1; // drop this timer! Notify user?!

		timers_head = tmp;
		timers_allocated += TIMERS_ALLOC_STEP;

	}


	// Insert sorted! If this was l-l, it'd be O(1). But then the
	// users of API could mess with ->next.
	for (i = 0; i < timers_inuse; i++) {
		int j;

		if (timercmp(&(node->when),
					 &(timers_head[i]->when),
					 < )) {

#ifdef DEBUG
			printf("[timers] inserting timer at pos %d\n", i);
#endif
			// Insert here. First make room by copying the remaining nodes.
			// Unfortunately, some platforms handle overlapping memcpy()
			// differently, so we can not use this.
			//memcpy(&timers_head[i+1], &timers_head[i],
			//	   (timers_inuse - i) * sizeof(*node));
			for (j = timers_inuse-1; j >= i; j--)
				timers_head[j+1] = timers_head[j];

			timers_head[i] = node;
			timers_inuse++;

			// Signal success.
			return 0;
		}

	} //for


#ifdef DEBUG
	printf("[timers] inserting timer at end %d\n", timers_inuse);
#endif

	// Just slap at end
	timers_head[timers_inuse] = node;
	timers_inuse++;


	// Signal success.
	return 0;

}






timers_t *timers_new(unsigned long major, unsigned long minor, int flags,
					 lion_timer_t callback, void *userdata)
{
	timers_t *newd;

	newd = timers_newnode();
	if (!newd) return newd;

	newd->major    = major;
	newd->minor    = minor;
	newd->flags    = flags;
	newd->callback = callback;
	newd->userdata = userdata;

	if (timers_add(newd)) {
		timers_freenode(newd);
		return NULL;
	}

	return newd;

}




int timers_cancel(timers_t *node)
{

	node->callback = NULL;
	return 1;

}




//
// Before calling select with a "timeout" we check the first timer in
// the queue (since its sorted, only need to check first) to see if it is
// due sooner that "timeout". If so, lower "timeout" to that value.
//
void timers_select(struct timeval *timeout)
{
	struct timeval nnow;

	if (!timers_inuse) return;

	gettimeofday(&nnow, NULL);

	// Add on fuzz
	timers_init();
	timeradd(&nnow, &fuzz, &nnow);

	// Check if it has already expired. (hence no underflow in math)
	if (timercmp(&(timers_head[0]->when),
				 &nnow,
				 <)) {
		timeout->tv_sec = 0;
		timeout->tv_usec = 0;
		return;
	}

	// Compute the diff
	timersub(&(timers_head[0]->when),
			 &nnow,
			 &nnow);

	// Check if timer is sooner than timeout.
	if (timercmp(&nnow,
				 timeout,
				 <)) {

		// Assign timeout to the smaller value.
		timeout->tv_sec  = nnow.tv_sec;
		timeout->tv_usec = nnow.tv_usec;

#ifdef DEBUG
		printf("[timers] lowering timeout to %lu.%lu sec/usec\n",
			   timeout->tv_sec, timeout->tv_usec);
#endif

	}

}



//
// select() call has finished, so let's check if the time expired is
// past any timers, if so, call them.
//
void timers_process(void)
{

	struct timeval nnow;
	timers_t *node;

	if (!timers_inuse) return;

	gettimeofday(&nnow, NULL);


	// Add on fuzz
	timers_init();
	timeradd(&nnow, &fuzz, &nnow);


	// While we have an expired timer...
	while (( timercmp(&nnow,
					 &(timers_head[0]->when),
					 >))) {

		node = timers_head[0];

		// Remove timer. It might be readded later.
		timers_inuse--;

		// Copy timers over if needed
		if (timers_inuse) {
			memcpy(&timers_head[0],
				   &timers_head[1],
				   timers_inuse * sizeof(*node));
		}

		// Was this timer already cancelled? If so,
		// free it, and move to the next timer.
		if (!node->callback) {
			timers_freenode(node);
			continue;
		}



#ifdef DEBUG
		printf("[timers] calling timer callback for %p : %d\n", node,
			   timers_inuse);
#endif
		// Call API for timer.
		node->callback(node, node->userdata);

		// If callback is NULL now, the timer was cancelled.
		// Free node or re-add
		if (!node->callback || !(node->flags & TIMERS_FLAG_REPEAT))
			timers_freenode(node);
		 else
			timers_add(node);

		// Don't loop if that was the last timer.
		if (!timers_inuse) break;

		// The next timer might be the same timer as we just called.
		if (node == timers_head[0]) break;


	} // while

}
