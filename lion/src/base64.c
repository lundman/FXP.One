
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

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <string.h>


static const unsigned char enc[] =
"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";


/*
 * Encode len bytes starting at clear using base64 encoding into encoded,
 * which should be at least ((len + 2) * 4 / 3 + 1) in size.
 * This was lifted from usr/bin/ftp/fetch.c - notice the change from
 * char * to unsigned char * as it was encoding incorrectly (negative index)
 * - Lundy
 */
void
base64_encode(clear, len, encoded)
	 const unsigned char  *clear;
	 int                   len;
	 unsigned char        *encoded;
{
	unsigned char    *cp;
	int      i;

	cp = encoded;
	for (i = 0; i < len; i += 3) {
	  *(cp++) = enc[((clear[i + 0] >> 2))];
	  *(cp++) = enc[((clear[i + 0] << 4) & 0x30)
					| ((clear[i + 1] >> 4) & 0x0f)];
	  *(cp++) = enc[((clear[i + 1] << 2) & 0x3c)
					| ((clear[i + 2] >> 6) & 0x03)];
	  *(cp++) = enc[((clear[i + 2]     ) & 0x3f)];
	}
	*cp = '\0';
	while (i-- > len)
	  *(--cp) = '=';
}



/*
 * Take a base64 encoded buffer, and decode it into to plain original format
 * Size is size of encoded buffer, 'plain' should contain enough space
 * to hold the result. ((size - 1) * 3 / 4 - 1)
 */
int base64_decode(unsigned char *encoded, unsigned char *plain, int size)
{
  unsigned char inalphabet[256], decoder[256];
  int i, bits, c=0, char_count, errors, incount, outcount;

  errors   = 0;
  incount  = 0;
  outcount = 0;

  memset(inalphabet, 0, sizeof( inalphabet ));
  memset(decoder, 0, sizeof( decoder ));

  for (i = (sizeof enc) - 1; i >= 0 ; i--) {
	inalphabet[(int)enc[i]] = 1;
	decoder[(int)enc[i]] = i;
  }

  char_count = 0;
  bits = 0;

  while (1) {

	if (incount >= size) break;

	c = encoded[incount++];

	if (c == '=')
	  break;

	if (c > 255 || ! inalphabet[c])
	  continue;

	bits += decoder[c];
	char_count++;

	if (char_count == 4) {

	  plain[outcount++] = (bits >> 16);
	  plain[outcount++] = ((bits >> 8) & 0xff);
	  plain[outcount++] = (bits & 0xff);
	  bits = 0;
	  char_count = 0;

	} else {
	  bits <<= 6;
	}
  }

  if (c != '=') {

	if (char_count) {

	  printf("base64 encoding incomplete: at least %d bits truncated",
			 ((4 - char_count) * 6));
	  errors++;
	}

  } else { /* c == '=' */
	switch (char_count) {
	case 1:
	  printf("base64 encoding incomplete: at least 2 bits missing");
	  errors++;
	  break;

	case 2:
	  plain[outcount++] = (bits >> 10);
	  break;

	case 3:
	  plain[outcount++] = (bits >> 16);
	  plain[outcount++] = ((bits >> 8) & 0xff);
	  break;
	}
  }

  return errors ? -1 : outcount;
}




