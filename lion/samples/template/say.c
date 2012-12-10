
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
#if __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif

#include "lion.h"

#include "object.h"

#include "say.h"








int say_all_sub( object_t *node, void *arg1, void *arg2 )
{
	// Out message should be in arg1, and strlen in arg2
	static char *line;
	static int len;
	
	if (!arg1 || !arg2)
		return 0; // 0 to abort iteration

	// Only say to registered connections
	if (node->type != OBJECT_TYPE_REGISTERED)
		return 1; // 1 to keep iterating.


	line = (char *) arg1;
	len = (int) arg2;

	lion_send(node->handle, line, len);

	return 1; // 1 to keep iterating.
}




//
// Send a string to a service
//
#if defined (__STDC__) || defined (WIN32)
int say_all(char const *fmt, ...)
#else
int say_all(fmt, va_alist)
     char const *fmt;
     va_dcl
#endif
{
  va_list ap;
  // Using a static buffer here is not ideal, but would be ok when
  // using "vsnprintf" - which is included with lion for Windows platforms.
  // Another option is using "lion_buffersize(YOUR_BUFFER_SIZE);" in your
  // main, then using that same value here.
  static char msg[4096];
  int result;

#if __STDC__ || WIN32
  va_start(ap, fmt);
#else
  va_start(ap);
#endif

  // Buffer overflow! No vsnprintf on many systems :(
  result = vsprintf(msg, fmt, ap);
  va_end(ap);


  result = strlen(msg);

  // Send it to all registered clients.
  object_find( say_all_sub, (void *)msg, (void *)result );

  // Tell console too. But \r\n makes double new line, so to make it
  // pretty, target and eliminate the "\r"
  if ((result > 2)  && 
	  (msg[ result - 2 ] == '\r'))
	  msg[ result - 2 ] = 0;

  puts(msg);

  return result;
                 
}

