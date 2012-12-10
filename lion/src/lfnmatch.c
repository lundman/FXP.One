
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

/* Copyright (C) 1991, 1992, 1993 Free Software Foundation, Inc.

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Library General Public License as
published by the Free Software Foundation; either version 2 of the
License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Library General Public License for more details.

You should have received a copy of the GNU Library General Public
License along with this library; see the file COPYING.LIB.  If
not, write to the Free Software Foundation, Inc., 675 Mass Ave,
Cambridge, MA 02139, USA.  */

//
// Lifted into LION because some OS don't have fnmatch, and worse, some
// only half implement it (See CASEFOLD).
//


#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <errno.h>
#include "lfnmatch.h"
#include <ctype.h>


/* Comment out all this code if we are using the GNU C Library, and are not
   actually compiling the library itself.  This code is part of the GNU C
   Library, but also included in many other GNU distributions.  Compiling
   and linking in this code is a waste when using the GNU C library
   (especially if it is a shared library).  Rather than having every GNU
   program understand `configure --with-gnu-libc' and omit the object files,
   it is simpler to just do this in the source for each such file.  */


#if !defined(__GNU_LIBRARY__) && !defined(STDC_HEADERS)
extern int errno;
#endif

/* Match STRING against the filename pattern PATTERN, returning zero if
   it matches, nonzero if not.  */
int
lfnmatch (pattern, string, flags)
     const char *pattern;
     const char *string;
     int flags;
{
  register const char *p = pattern, *n = string;
  register char c;

/* Note that this evalutes C many times.  */
#define FOLD(c)	((flags & LFNM_CASEFOLD) && isupper ((unsigned char)c) ? tolower ((unsigned char)c) : (c))

  while ((c = *p++) != '\0')
    {
      c = FOLD (c);

      switch (c)
	{
	case '?':
	  if (*n == '\0')
	    return LFNM_NOMATCH;
	  else if ((flags & LFNM_FILE_NAME) && *n == '/')
	    return LFNM_NOMATCH;
	  else if ((flags & LFNM_PERIOD) && *n == '.' &&
		   (n == string || ((flags & LFNM_FILE_NAME) && n[-1] == '/')))
	    return LFNM_NOMATCH;
	  break;

	case '\\':
	  if (!(flags & LFNM_NOESCAPE))
	    {
	      c = *p++;
	      c = FOLD (c);
	    }
	  if (FOLD (*n) != c)
	    return LFNM_NOMATCH;
	  break;

	case '*':
	  if ((flags & LFNM_PERIOD) && *n == '.' &&
	      (n == string || ((flags & LFNM_FILE_NAME) && n[-1] == '/')))
	    return LFNM_NOMATCH;

	  for (c = *p++; c == '?' || c == '*'; c = *p++, ++n)
	    if (((flags & LFNM_FILE_NAME) && *n == '/') ||
		(c == '?' && *n == '\0'))
	      return LFNM_NOMATCH;

	  if (c == '\0')
	    return 0;

	  {
	    char c1 = (!(flags & LFNM_NOESCAPE) && c == '\\') ? *p : c;
	    c1 = FOLD (c1);
	    for (--p; *n != '\0'; ++n)
	      if ((c == '[' || FOLD (*n) == c1) &&
		  lfnmatch (p, n, flags & ~LFNM_PERIOD) == 0)
		return 0;
	    return LFNM_NOMATCH;
	  }

	case '[':
	  {
	    /* Nonzero if the sense of the character class is inverted.  */
	    register int not;

	    if (*n == '\0')
	      return LFNM_NOMATCH;

	    if ((flags & LFNM_PERIOD) && *n == '.' &&
		(n == string || ((flags & LFNM_FILE_NAME) && n[-1] == '/')))
	      return LFNM_NOMATCH;

	    not = (*p == '!' || *p == '^');
	    if (not)
	      ++p;

	    c = *p++;
	    for (;;)
	      {
		register char cstart = c, cend = c;

		if (!(flags & LFNM_NOESCAPE) && c == '\\')
		  cstart = cend = *p++;

		cstart = cend = FOLD (cstart);

		if (c == '\0')
		  /* [ (unterminated) loses.  */
		  return LFNM_NOMATCH;

		c = *p++;
		c = FOLD (c);

		if ((flags & LFNM_FILE_NAME) && c == '/')
		  /* [/] can never match.  */
		  return LFNM_NOMATCH;

		if (c == '-' && *p != ']')
		  {
		    cend = *p++;
		    if (!(flags & LFNM_NOESCAPE) && cend == '\\')
		      cend = *p++;
		    if (cend == '\0')
		      return LFNM_NOMATCH;
		    cend = FOLD (cend);

		    c = *p++;
		  }

		if (FOLD (*n) >= cstart && FOLD (*n) <= cend)
		  goto matched;

		if (c == ']')
		  break;
	      }
	    if (!not)
	      return LFNM_NOMATCH;
	    break;

	  matched:;
	    /* Skip the rest of the [...] that already matched.  */
	    while (c != ']')
	      {
		if (c == '\0')
		  /* [... (unterminated) loses.  */
		  return LFNM_NOMATCH;

		c = *p++;
		if (!(flags & LFNM_NOESCAPE) && c == '\\')
		  /* XXX 1003.2d11 is unclear if this is right.  */
		  ++p;
	      }
	    if (not)
	      return LFNM_NOMATCH;
	  }
	  break;

	default:
	  if (c != FOLD (*n))
	    return LFNM_NOMATCH;
	}

      ++n;
    }

  if (*n == '\0')
    return 0;

  if ((flags & LFNM_LEADING_DIR) && *n == '/')
    /* The LFNM_LEADING_DIR flag says that "foo*" matches "foobar/frobozz".  */
    return 0;

  return LFNM_NOMATCH;
}



