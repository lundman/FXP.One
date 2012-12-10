
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

// $Id: lgroup.c,v 1.2 2007/12/17 08:03:59 lundman Exp $
// Lgroup rate limits.
// Jorgen Lundman April 20th, 2003

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "connections.h"
#include "lion.h"
#include "lgroup.h"






static THREAD_SAFE lgroup_t *lgroup_list      = NULL;
static THREAD_SAFE int    lgroup_allocated = 0;
static THREAD_SAFE int    lgroup_used      = 0;








//
// One shot exit to release lgroups. Probably only called at lion_exit time.
//
void lgroup_exit(void)
{

	if (lgroup_list) {

		free(lgroup_list);
		lgroup_list = NULL;

		lgroup_allocated = 0;
		lgroup_used = 0;

	}

}





//
// Return a new lgroup, a lgroup is free if its rate is 0.
//
int lgroup_new(void)
{
	lgroup_t *newg;
	int i;

	// We may need to grow the list.
	if (lgroup_used >= lgroup_allocated) {

		newg = realloc(lgroup_list, 
					   sizeof(lgroup_t) * (lgroup_allocated + LGROUP_INCREMENT));

		// Failed? uhoh
		if (!newg) {
			return -1;
		}

		// Make sure the new nodes are NULL'ed.
		memset( &newg[ lgroup_allocated ], 0, 
				sizeof(lgroup_t) * LGROUP_INCREMENT);

		lgroup_list = newg;
		lgroup_allocated += LGROUP_INCREMENT;

	}

	// Now go pick out a new node.
	// btw, this can be made to be more efficient

	// 0 is global.
	for (i = 1; i < lgroup_allocated; i++) {

		if (lgroup_list[ i ].used == 0) {  

			// This one is ours! We need to set it to some non-zero value
			// so it is thought of as used.
			lgroup_list[ i ].used = 1;

			return i;

		}

	}

	printf("[lgroup] internal error in lgroup_new. No free nodes?!\n");
	return -1;

}



//
// Release a lgroup, set its rate to 0.
//
void lgroup_free(int gid)
{
	
	if ((gid >= 0) && (gid < lgroup_allocated)) {
		
		lgroup_list[ gid ].used = 0;
		
	}
	
}



void lgroup_set_rate_in(int gid, int cps) 
{
	// If they want to set global, but we've not allocated any yet, lets do so
	if ((gid == LGROUP_GLOBAL) && !lgroup_allocated) {

		lgroup_free( lgroup_new() );

	}

	// Valid gid?
	if ((gid < 0) || (gid >= lgroup_allocated))
		return;

	lgroup_list[ gid ].cps_in = cps;

}



void lgroup_set_rate_out(int gid, int cps) 
{
	// If they want to set global, but we've not allocated any yet, lets do so
	if ((gid == LGROUP_GLOBAL) && !lgroup_allocated) {

		lgroup_free( lgroup_new() );

	}

	// Valid gid?
	if ((gid < 0) || (gid >= lgroup_allocated))
		return;

	lgroup_list[ gid ].cps_out = cps;

}





//
// Attach a lgroup to a lion_t handle.
//
// FIXME!! Multilgroup support!
void lgroup_add(connection_t *handle, int gid)
{
	// Valid gid?
	if ((gid < 0) || (gid >= lgroup_allocated))
		return;


	handle->group = gid;

}



// FIXME!! Multilgroup support!
void lgroup_remove(connection_t *handle, int gid)
{

	if (handle->group == gid)
		handle->group = 0;    // 0 is global so safe to set it to that.

}



void lgroup_add_in(connection_t *node, int bytes)
{

	if (!lgroup_allocated || !lgroup_list)
		return;

	// If a group is set, add to it:
	if ((node->group > 0) &&
		(node->group < lgroup_allocated)) {

		lgroup_list[ node->group ].current_in += bytes;

	}

	// Check if global is set:
	if (lgroup_list[ LGROUP_GLOBAL ].cps_in) {

		lgroup_list[ LGROUP_GLOBAL ].current_in += bytes;

	}

}


void lgroup_add_out(connection_t *node, int bytes)
{

	if (!lgroup_allocated || !lgroup_list)
		return;

	// If a group is set, add to it:
	if ((node->group > 0) &&
		(node->group < lgroup_allocated)) {

		lgroup_list[ node->group ].current_out += bytes;

	}

	// Check if global is set:
	if (lgroup_list[ LGROUP_GLOBAL ].cps_out) {

		lgroup_list[ LGROUP_GLOBAL ].current_out += bytes;

	}

}



// 0 means DON'T add to fd_set, and deny traffic
// none-zero means still within limit.
// This is called often, could make it faster, maybe #define.
int lgroup_check_rate_in(connection_t *node, int gid)
{
	int rate;
	//	printf("[lgroup] rate_in\n");

	// In valid group?
	// Now passed along...
	//gid = node->group;

	if ((gid < 0) || (gid >= lgroup_allocated))
		return 1;

	
	// We do this in order of efficiency
	// First check connection rate...
	rate = lgroup_list[ gid ].cps_in * 1024;


	if (rate) {

		// New time cycle?
		if (lgroup_list[ gid ].stamp_in < lion_global_time) {
			lgroup_list[ gid ].stamp_in       = lion_global_time;
			lgroup_list[ gid ].current_in  = 0;
		}

		if (lgroup_list[ gid ].current_in > rate) {
			return 0;
		}


	}


	return 1;
}


// 0 means DON'T add to fd_set, and deny traffic
// none-zero means still within limit.
// This is called often, could make it faster, maybe #define.
int lgroup_check_rate_out(connection_t *node, int gid)
{
	int rate;

	if ((gid < 0) || (gid >= lgroup_allocated))
		return 1;

	
	// We do this in order of efficiency
	// First check connection rate...
	rate = lgroup_list[ gid ].cps_out * 1024;

	if (rate) {
		
		// New time cycle?
		if (lgroup_list[ gid ].stamp_out < lion_global_time) {
			lgroup_list[ gid ].stamp_out       = lion_global_time;
			lgroup_list[ gid ].current_out = 0;
		}


		if (lgroup_list[ gid ].current_out > rate) {
			return 0;
		}


	}


	return 1;
}
