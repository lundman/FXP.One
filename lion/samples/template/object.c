
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

#include <stdio.h>
#include <stdlib.h>


#include "object.h"



// Start of our linked list. It is made static so it can not be touched
// outside of this module. You should call the functions provided.

static object_t *object_listhead = NULL;







object_t *object_new(void)
{
	object_t *retval;


	retval = (object_t *)malloc(sizeof(*retval));

	if (!retval) {
		perror("object_new");
		exit(-1);
	}


	// Clear the node, always good practise.
	memset(retval, 0, sizeof(*retval));


	// Insert into linked list. 
	// For this sample, a linked list is never really required but
	// I have included it as an example, and therefor we can do
	// object_free_all, as well as a "iterate all connected" command.
	retval->next = object_listhead;
	object_listhead = retval;

	// Return it to the caller
	return retval;

}





void object_free(object_t *node)
{
	object_t *prev, *runner;
	
	if (node->username) {
		free( node->username );
		node->username = NULL;
	}
	
	
	// Remove it from the linked list.
	
	for (prev = NULL, runner = object_listhead;
		 runner;
		 prev = runner, runner = runner->next) {
		
		
		if (runner == node) {  // This is the one to remove
			
			if (!prev) {
				
				// If no previous node, then it's at the start of the list
				
				object_listhead = runner->next;
				
			} else {  
				
				// In the middle somewhere
				
				prev->next = runner->next;
				
			}
			
			break;  // Stop spinning in the for-loop. 
			
		} // if runner == node
		
	} // for
	
  
	// Release it
	free( node );

}



void object_free_all(void)
{

	// Since we call object_free() which re-arranges the linked-list
	// we can't just iterate it. So, slightly different semantics.

	while( object_listhead ) {

		object_free( object_listhead );

	}


}



//
// Find a particular node, or, iterate the list.
//
// IN:  comparison function, expected to return 0 on match
// IN:  two optional arguments that are simply passed on
// OUT: pointer to matching node, or NULL if end reached
//
object_t *object_find( int (*compare)(object_t *, void *, void *),
					   void *optarg1, void *optarg2)
{
  object_t *runner;

  for (runner = object_listhead; runner; runner = runner->next) {

	  if (!compare(runner, optarg1, optarg2)) {
		  
		  // Found it, apparently
		  return runner;
		  
	  }
	  
  }
  
  // Didn't find it
  return NULL;
  
}

