
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

/*
 * timers example
 *
 * by Jorgen Lundman <lundman@lundman.net> 
 *
 * 
 * 
 * 27/12/2005 - epoch
 *
 */

#include <stdio.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>

#include "lion.h"


static int master_switch = 0;
static struct timeval start_time;


int lion_userinput( lion_t *handle, 
					void *user_data, int status, int size, char *line)
{
	
		
	switch( status ) {

	}
	


	return 0;

}








void exit_interrupt(void)
{

	master_switch = 1;

}





lion_timer_t test_timer_1(timers_t *timer, void *userdata)
{
	struct timeval diff;
	float secs;

	gettimeofday(&diff, NULL);

	//	timersub(&start_time, &diff, &diff);
	timersub(&diff, &start_time, &diff);
	secs =  (float) diff.tv_sec;
	secs += (float)diff.tv_usec / 1000000.0;

	printf("timer1 triggered (should be second): after %lu/%lu sec/usec. %2.7f\n",
		   diff.tv_sec, diff.tv_usec, secs);

}

lion_timer_t test_timer_2(timers_t *timer, void *userdata)
{
	struct timeval diff;
	float secs;
	gettimeofday(&diff, NULL);
	//timersub(&start_time, &diff, &diff);
	timersub(&diff, &start_time, &diff);

	secs =  (float) diff.tv_sec;
	secs += (float)diff.tv_usec / 1000000.0;
	printf("timer2 triggered (should be first): after %lu/%lu sec/usec. %2.7f\n",
		   diff.tv_sec, diff.tv_usec, secs);
}

lion_timer_t test_timer_3(timers_t *timer, void *userdata)
{
	struct timeval diff;
	float secs;
	static int count = 0;

	gettimeofday(&diff, NULL);
	//	timersub(&start_time, &diff, &diff);
	timersub(&diff, &start_time, &diff);
	secs =  (float) diff.tv_sec;
	secs += (float)diff.tv_usec / 1000000.0;

	printf("timer3 triggered (should be every 5s): after %lu/%lu sec/usec. %2.7f\n",
		   diff.tv_sec, diff.tv_usec, secs);

	count++;
	if (count > 10) {
		printf("Cancelling timer.\n");
		timers_cancel(timer);
	}

}





int main(int argc, char **argv)
{

	signal(SIGINT, exit_interrupt);
#ifndef WIN32
	signal(SIGHUP, exit_interrupt);
#endif
	


	printf("Initialising Network...\n");

	lion_init();
	lion_compress_level( 0 );

	printf("Network Initialised.\n");

	
	printf("Arming timers...\n");
	gettimeofday(&start_time, NULL);

	// Add in some timers. 
	// These flags are not needed, "0" would also work.
	timers_new(4, 0, TIMERS_FLAG_RELATIVE | TIMERS_FLAG_ONESHOT, 
			   test_timer_1, NULL);

	// Insert 2nd timer to trigger first.
	timers_new(2, 50000, 0,
			   test_timer_2, NULL);

	// Finally, repeating timer every 5.54 seconds.
	timers_new(5, 900, TIMERS_FLAG_REPEAT,
			   test_timer_3, NULL);


	printf("...waiting..\n");
	while( !master_switch ) {

		lion_poll(0, 1);     // This blocks. (by choice, FYI).

	}
	printf("\n");

	lion_free();

	printf("Network Released.\n");

	printf("Done\n");

	return 0;

}
