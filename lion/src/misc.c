
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

// $Id: misc.c,v 1.8 2011/10/18 08:49:33 lundman Exp $
// Miscellaneous utility functions
// Jorgen Lundman November 25th, 1999
// Copyright (c) 1999 HotGen Studios Ltd <www.hotgen.com>

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <ctype.h>

#include "misc.h"
#include "lion.h"
//#include "fnmatch.h"


// Filled in with the matched character
char misc_digtoken_optchar = 0;

#if 1 // new and handles quotes
char *misc_digtoken(char **string,char *match)
{

	misc_digtoken_optchar = 0;

	if(string && *string && **string) {

		// Skip "whitespace", anything in 'match' until we have something
		// that doesn't.
		// However, if we hit a " we stop asap, and match only to next ".


		while(**string && strchr(match,**string)) {

			// If a char is the quote, and we match against quote, stop loop
			(*string)++;

		}


		//printf(" found %c\n", **string);
		// If the first char is now a quote, and we match against quotes:
		if (!strchr(match, '"') && (**string == '"')) {

			//printf("  hack\n");
			// Skip the quote we are on
			(*string)++;

			// change match to only have a quote in it, that way we will
			// only match another quote.
			match = "\"";

		}


		if(**string) { /* got something */
			char *token=*string;

			if((*string=strpbrk(*string,match))) {

				misc_digtoken_optchar = *(*string);

				// If we match " to we force it to only look for
				// matching " and not any of the alternatives?

				*(*string)++=(char)0;
				while(**string && strchr(match,**string))
					(*string)++;

			}  else
				*string = ""; /* must be at the end */

			return(token);
		}
	}
	return((char *)0);
}

#else

char *misc_digtoken(char **string,char *match)
{
  if(string && *string && **string) {

    // Skip "whitespace", anything in 'match' until we have something
    // that doesn't.
    // However, if we hit a " we stop asap, and match only to next ".

    while(**string && strchr(match,**string)) {

      if (**string == '"') {

	(*string)++;
	match = "\"";
	break;

      }

      (*string)++;

    }

    if(**string) { /* got something */
      char *token=*string;

      if((*string=strpbrk(*string,match))) {

	misc_digtoken_optchar = *(*string);

	// If we match " to we force it to only look for
	// matching " and not any of the alternatives?

        *(*string)++=(char)0;
	while(**string && strchr(match,**string))
          (*string)++;

      }  else
        *string = ""; /* must be at the end */

      return(token);
    }
  }
  return((char *)0);
}

#endif


//
// If you want to check out unreadable code, check this out
//
int mystrccmp(register char *s1,register char *s2) {

  while((((*s1)>='a'&&(*s1)<='z')?(*s1)-32:*s1)==(((*s2)>='a'&&(*s2)<='z')?(*s2++)-32:*s2++))
    if(*s1++==0) return 0;
  return (*(unsigned char *)s1-*(unsigned char *)--s2);
}



char *mystrcpy(char *s)
{
  char *r;

  r = (char *) malloc(strlen(s)+1);
  if (!r) {
    perror("mystrcpy()");
    exit(1);
  }

  strcpy(r, s);
  return r;
}



// switch uses jump tables so they are O(0)
#define CONVCHAR(X, Y) switch((X)) {\
                    case '1':    \
                    case '2':    \
                    case '3':    \
                    case '4':    \
                    case '5':    \
                    case '6':    \
                    case '7':    \
                    case '8':    \
                    case '9':    \
                    case '0':    \
                      (Y)=(X)-'0';   \
                      break;     \
                    case 'a':    \
                    case 'b':    \
                    case 'c':    \
                    case 'd':    \
                    case 'e':    \
                    case 'f':    \
                      (Y)=(X)-'a'+10;\
                      break;     \
                    case 'A':    \
                    case 'B':    \
                    case 'C':    \
                    case 'D':    \
                    case 'E':    \
                    case 'F':    \
                      (Y)=(X)-'A'+10;\
                      break;     \
                    }


//
// Convert a hex string "4f32109fae" into the equivalent binary, return
// number of bytes
//
int  misc_hextobin(char *hex)
{
  unsigned char high=0, low=0;
  int result, out;

  result = 0;
  out = 0;

  while (hex[out]) {

    CONVCHAR(hex[out], high);
    out++;
    CONVCHAR(hex[out], low);
    out++;

    hex[result] = (high << 4) + low;
    result ++;

  }

  return result;

}


//
// Convert binary into hex string, reverse order so we can re-use memory
//
int  misc_bintohex(char *bin, int len)
{
  unsigned char hex, low, high;
  unsigned char table[16] =
    {'0','1','2','3','4','5','6','7','8','9','a','b','c','d','e','f'};

  bin[len * 2] = 0; // terminate string


  for (len--; len >= 0; len--) {

    hex = bin[ len ];

    low = hex & 0x0f;
    high = (hex & 0xf0) >> 4;

    bin[ len * 2 + 1] = table[ low ];
    bin[ len * 2 ]    = table[ high  ];

  }

  return 0;

}



char *misc_strdup(char *s)
{
  char *result;

  result = (char *) malloc( strlen( s ) +1 );

  if (result)
    strcpy(result, s);

  return result;

}



time_t misc_getdate(char *date)
{
  struct tm tmw, *tmn;
  char *ar, *token, *r;
  int i;
  static char months[12][4] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun",
				"Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

  // It is either
  // 'Nov 10 11:44'   for less than 6mnths old or
  // 'Mar 27  1999'   for more.

  memset(&tmw, 0, sizeof(tmw));
  ar = date;


  // read month
  if (!(token = misc_digtoken(&ar, " "))) goto getdate_error;

  for (i = 0; i < 12; i++)
    if (!mystrccmp(months[i], token)) {
      tmw.tm_mon = i;
      break;
    }

  if (i == 12) goto getdate_error;


  // get day of month
  if (!(token = misc_digtoken(&ar, " "))) goto getdate_error;

  tmw.tm_mday = atoi(token);


  // get either time, or, year
  if (!(token = misc_digtoken(&ar, " "))) goto getdate_error;


  if ((r = strchr(token, ':'))) {

    // it's time
    *r = 0;

	tmn = localtime(&lion_global_time);

    if (!tmn) goto getdate_error;

    tmw.tm_hour = atoi(token);
    tmw.tm_min  = atoi(&r[1]);
    tmw.tm_year = tmn->tm_year;


    // Ok, if we are in January, but the Month read was Dec or
    // earlier, we need to go -1 on the year. I think this can be
    // expressed as now_month - read_month < -6
    if ((tmn->tm_mon - tmw.tm_mon) <= -6)
      tmw.tm_year -= 1;


  } else {

    // It's the year
    tmw.tm_year = atoi(token) - 1900; // as according to the man page

  }


  // Convert it into a time_t, well, as close as possible.
  return mktime(&tmw);

 getdate_error:
  return 0;

}


char *misc_idletime(time_t time)
{ /* This support functionw as written by George Shearer (Dr_Delete) */

  THREAD_SAFE static char workstr[100];
  unsigned short int days=(time/86400),hours,mins,secs;
  hours=((time-(days*86400))/3600);
  mins=((time-(days*86400)-(hours*3600))/60);
  secs=(time-(days*86400)-(hours*3600)-(mins*60));

  workstr[0]=(char)0;
  if(days)
	  snprintf(workstr,sizeof(workstr), "%dd",days);
  snprintf(workstr,sizeof(workstr),"%s%s%02d",workstr,(workstr[0])?":":"",hours);
  snprintf(workstr,sizeof(workstr),"%s%s%02d",workstr,(workstr[0])?":":"",mins);
  snprintf(workstr,sizeof(workstr),"%s%s%02d",workstr,(workstr[0])?":":"",secs);
  if (!days && !hours && !mins && !secs)
    snprintf(workstr,sizeof(workstr),"0 seconds");

  return(workstr);
}

char *misc_idletime2(time_t time)
{ /* This support functionw as written by George Shearer (Dr_Delete) */

  THREAD_SAFE static char workstr[100];
  unsigned short int days=(time/86400),hours,mins,secs;
  hours=((time-(days*86400))/3600);
  mins=((time-(days*86400)-(hours*3600))/60);
  secs=(time-(days*86400)-(hours*3600)-(mins*60));

  workstr[0]=(char)0;
  if(days)
	  snprintf(workstr,sizeof(workstr), "%dd",days);
  snprintf(workstr,sizeof(workstr),"%s%s%02d",workstr,(workstr[0])?":":"",hours);
  snprintf(workstr,sizeof(workstr),"%s%s%02d",workstr,(workstr[0])?":":"",mins);
  snprintf(workstr,sizeof(workstr),"%s%s%02d",workstr,(workstr[0])?":":"",secs);
  if (!days && !hours && !mins && !secs)
    snprintf(workstr,sizeof(workstr),"0 seconds");

  return(workstr);
}


#if 0
char *misc_makepath(char *cwd, char *file)
{
  static char result[8192];   // Buffer over flow
  int i;

  if (!file) {  // dir up instead

    strcpy(result, cwd);
    // If we end with '/' we truncate those.
    i = strlen(result) - 1;
    while ((i >= 0) && result[i] == '/')
      result[i--] = 0;

    // Now seek back until we find a '/'
    while ((i >= 0) && result[i] != '/') i--;

    if (result[i] == '/') result[i] = 0;

    if ((i <= 0) || !result[0]) {
      result[0] = '/';
      result[1] = 0;
    }

#ifdef WIN32

    // In windows, if we've gone back to just "X:" make it "X:/"
    if ((result[1] == ':') &&
	(result[2] == 0) ) {

      result[2] = '/';
      result[3] = 0;

    }

#endif

    return result;
  }


  // If cwd ends with '/' don't add one.
  if ( cwd[ strlen(cwd) - 1 ] == '/')
    sprintf(result, "%s%s", cwd, file);
  else
    sprintf(result, "%s/%s", cwd, file);

  return result;
}
#endif



char *misc_pasv2port(char *pasv)
{
  THREAD_SAFE static char result[1024];
  char *r;

  // Always terminate it, since strncpy might not.
  result[sizeof(result)-1] = 0;

  *result = 0;

  r = strchr(pasv, '(');

  if (r) {

	  strncpy(result, &r[1], sizeof(result)-2);

    r = strchr(result, ')');

    if (r) *r = 0;

  }

  return result;

}





char *misc_strjoin(char *a, char *b)
{
  char *result;
  int len_a, extra_slash;

  // Check if a ends with / or, b starts with /, if so dont
  // add a '/'.
  len_a = strlen(a);

  // If neither a ends with / AND b don't start with slash, add slash.
  if ((a[ len_a - 1 ] != '/') && b[0] != '/')
    extra_slash = 1;
  else
    extra_slash = 0;

  // If both have slashes, skip all leading 'b' slashes, and only use a.
  if ((a[ len_a - 1 ] == '/'))
      while(*b=='/') b++;


  // misc_strjoin('720/' + '/Music' -> '720//Music'

  result = (char *) malloc(len_a + strlen(b) + 1 + extra_slash);

  if (!result) {
    perror("malloc");
    exit(2);
  }

  if (extra_slash)
    sprintf(result, "%s/%s", a, b);
  else
    sprintf(result, "%s%s", a, b);

  return result;
}


char *misc_strjoin_plain(char *a, char *b)
{
  char *result;
  int len_a;

  if (!a || !b) return NULL;

  // Check if a ends with / or, b starts with /, if so dont
  // add a '/'.
  len_a = strlen(a);

  result = (char *) malloc(len_a + strlen(b) + 1 );

  if (!result) {
    perror("malloc");
    return NULL;
  }

  sprintf(result, "%s%s", a, b);

  return result;
}

#if 0
int   misc_skipfile(char *patterns, char *name)
{
  char *tmp;
  char *pattern, *ar;

  tmp = strdup(patterns);
  ar = tmp;

  while (( pattern = misc_digtoken(&ar, " \""))) {


    if (!fnmatch(pattern, name, FNM_CASEFOLD)) {
      free(tmp);
      return 1;

    }


  }


  free( tmp );

  return 0;
}
#endif


void misc_stripslash(char *s)
{
  int l;

  l = strlen(s) - 1;

  while ((l > 2) && (s[l] == '/')) s[l--] = 0;

}



void misc_undos_path(char *path)
{
  char *r;

  while ( (r = strchr(path, '\\')) )
    *r = '/';


}



char *misc_strip(char *s)
{
  char *r;

  if ((r = strchr(s, '\r'))) *r = 0;
  if ((r = strchr(s, '\n'))) *r = 0;

  return s;
}



//
// Take a string, and allocate a new string which is URL encoded,
// even if no encoding was needed, it is allocated.
// Is it worth running through once to know how much to allocate, or
// just allocate * 3 ? But to allocate *3 I need to call strlen anyway.
// (Although, some OS can do that based on longs)
//
// Only alphanumerics [0-9a-zA-Z], the special characters "$-_.!*'(),"
// Please note this is the QUERY STRING encoding. This means "+" is
// incoded as %2B and " " (space) is encoded as "+".
//
// As per RFC 1630 (RFC1630) "Universal Resource Identifiers in WWW: A Unifying Syntax for the Expression of Names and Addresses of Objects on the Network as used in the World-Wide Web"
//
char *misc_url_encode(char *s)
{
	char *result = NULL;
	int len = 0;
	char *r, *r2=NULL;
	static char hex[] = "0123456789ABCDEF";

	// How much space to allocate?
	while(1) {

		r = s;

		while (*r) {

			switch(tolower(*r)) {
			case 'a':
			case 'b':
			case 'c':
			case 'd':
			case 'e':
			case 'f':
			case 'g':
			case 'h':
			case 'i':
			case 'j':
			case 'k':
			case 'l':
			case 'm':
			case 'n':
			case 'o':
			case 'p':
			case 'q':
			case 'r':
			case 's':
			case 't':
			case 'u':
			case 'v':
			case 'w':
			case 'x':
			case 'y':
			case 'z':
			case '0':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
			case '$':
			case '-':
			case '_':
			case '.':
			case '!':
			case '*':
			case '\'':
			case '(':
			case ')':
			case ',':
			case '/': // and slash man!
				len++;
				if (result) *r2++ = *r;
				break;
			case ' ': // space becomes plus
				len++;
				if (result) *r2++ = '+';
				break;
			default:
				len+=3;
				if (result) {
					*r2++ = '%';
					*r2++ = hex[((*r>>4)&0x0f)];
					*r2++ = hex[((*r)&0x0f)];
				}
			} // switch
			r++;
		} // while r

		if (result) {
			*r2 = 0;
			break;
		}

		if (!len) break;

		result = malloc(len + 1);

		if (!result) break;

		r2 = result;

	} // while 1

	return result;

}




//
// Decode the url, allocate new area
//
char *misc_url_decode(char *s)
{
	char *result, *r, chr;

	result = strdup(s);

	if (!result) return NULL;

	r = result;

	while(*s) {

		if (*s != '%') {
			if (*s == '+') {  // plus becomes space
				*r++ = ' ';
				s++;
			} else {
				*r++ = *s++;  // copy verbatim
			}
		} else {              // otherwise, decode..
			chr = 0;
			s++;

			if (!*s) break;

			if ((toupper(*s) >= 'A') &&
				(toupper(*s) <= 'F'))
				chr = toupper(*s) - 'A' + 0x0A;
			else if ((*s >= '0') &&
					 (*s <= '9'))
				chr = *s - '0';
			s++;
			chr <<= 4;
			if ((toupper(*s) >= 'A') &&
				(toupper(*s) <= 'F'))
				chr |= (toupper(*s) - 'A' + 0x0A) & 0x0f;
			else if ((*s >= '0') &&
					 (*s <= '9'))
				chr |= (*s - '0') & 0x0f;
			s++;
			*r++ = chr;

		} // %

	}

	*r = 0;

	return result;

}

#ifndef HAVE_STRCASESTR
#include <ctype.h>
#include <string.h>
char *strcasestr (char *h, char *n)
{
    char *hp, *np = n, *match = 0;

    if(!*np) {
        return hp;
    }

    for (hp = h; *hp; hp++) {
        if (toupper(*hp) == toupper(*np)) {
            if (!match) {
                match = hp;
            }
            if(!*++np) {
                return match;
            }
        } else {
            if (match) {
                match = 0;
                np = n;
            }
        }
    }

    return NULL;
}
#endif

