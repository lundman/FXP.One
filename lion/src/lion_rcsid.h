
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

#ifndef LION_RCSID_H_INCLUDED
#define LION_RCSID_H_INCLUDED


//
// Latestly, we define a way to make tags. This is done the same way NetBSD
// does it, only to be compatible. Aren't I nice.



# ifndef GCC_UNUSED
#  if __GNUC__ >= 2
#   if __GNUC__ == 2 && __GNUC_MINOR__ < 7
#    define GCC_UNUSED(decl) decl
#   else /* __GNUC__ == 2 && __GNUC_MINOR__ < 7 */
#    define GCC_UNUSED(decl) decl __attribute__((__unused__))
#   endif /* __GNUC__ == 2 && __GNUC_MINOR__ < 7 */
#  else /* __GNUC__ >= 2 */
#   define GCC_UNUSED(decl) decl
#  endif /* __GNUC__ >= 2 */
# endif /* SM_UNUSED */

// If we couldn't define it previously (Win32 etc) define it now.
#ifndef GCC_UNUSED
#define GCC_UNUSED(X) X
#endif



# ifndef __RCSID
#  define __RCSID(str) GCC_UNUSED(static const char RcsId[]) = str;
# endif




#endif // INCLUDED
