// $Id: file.c,v 1.12 2012/08/11 00:15:19 lundman Exp $
// 
// Jorgen Lundman 18th March 2004.
//
// 
//


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "lion.h"
#include "misc.h"
#include "lfnmatch.h"

#include "debug.h"
#include "settings.h"
#include "file.h"



file_t *file_parse(char *line, int raw)
{
	file_t *result;
	char *ar, *perm, *thingy, *user, *group, *size, *name, *month, *day, *timeyear;
	char date[14];
	yesnoauto_t directory = YNA_NO;
	yesnoauto_t soft_link = YNA_NO;

	//debugf("  [file] '%s'\n", line);

	if (!line) return NULL;

	result = (file_t *) malloc(sizeof(*result));
	if (!result) return NULL;

	memset(result, 0, sizeof(*result));
	
	// Copy the line if they requested it.
	if (raw) 
		result->raw = strdup(line);


	// drwxr-xr-x   7  root     root         1024 Mar  3 18:36 ..
	// -rw-r--r--   1  ftp      ftp             5 Nov 10 11:42 Return
	// drwxr-xr-x F0   lftpd    lftpd           0 Mar 27  1999 Cd2
	// -rw-r--r-- D0   lftpd    lftpd           0 Mar 27  1999 sample.j
	// drwxrwxrwx   1  user     group           0 Jun 22  1999 old
	// -rwxrwxrwx   1  user     group     5327872 Nov 30  1998 q2-3.20-x86.exe
	// dr-xr-xr-x   1  owner    group           0 Feb 25  0:31 developr
	// -r-xr-xr-x   1  owner    group        7983 Jan 28  1999 dirmap.htm
	//
	// drwxr-xr-x    1 cubnc     cubnc 512 Aug 7 7:54 TV-TODAY
	// drwxr-xr-x    1 cubnc     cubnc 512 Jul 31 6:36 0c                        3.7 Terrabytes Online
	// drwxr-xr-x    1 cubnc     cubnc 512 Jul 31 6:36 MUSICDVDR


	// Bollox, some FTP sites don't have that initial argument.
	//

	ar = (char *)line;
	
	if (!(perm = misc_digtoken(&ar, " \t\r\n"))) goto parse_error;

	if (!mystrccmp("total", perm)) {  // it's the "total XX" line, ignore it.
		file_free(result);
		return NULL;
	}

	if (strlen(perm) != 10) { // We assume it's always this length
		debugf("[%s] Unknown data, permission section isn't strlen 10 :'%s' - '%s'\n",
			   "file", perm, ar);
		goto parse_error;		
	}
	
	

	// Parse out the rest
	
	if (!(thingy = misc_digtoken(&ar, " \t\r\n"))) goto parse_error;
	if (!(user   = misc_digtoken(&ar, " \t\r\n"))) goto parse_error;
	if (!(group  = misc_digtoken(&ar, " \t\r\n"))) goto parse_error;
	if (!(size   = misc_digtoken(&ar, " \t\r\n"))) goto parse_error;
	

	// Bollox!
	// Here, if size is actually a month "Feb" etc, we have 
	// one less argument than expected!!
	// So we try to back track somewhat...
	//
	if ((strlen(size) == 3) &&
		isalpha(size[0]) &&
		isalpha(size[1]) &&
		isalpha(size[2])) {
		
		// Back the date into area, decrementing a char * hmmm :)
		*(--ar) = ' ';
		*(--ar) = size[2];
		*(--ar) = size[1];
		*(--ar) = size[0];
		
    size  = group;
    group = user;
    user  = thingy;
	
	}


	while (*ar == ' ') ar++;  // skip any whitespace to the date section.

	// Ok, we can not assume date is always 12 chars. Shame.
	// "Jan 28  1999"
	// "Aug 7 7:54"

	if (!(month    = misc_digtoken(&ar, " \t\r\n"))) goto parse_error;
	if (!(day      = misc_digtoken(&ar, " \t\r\n"))) goto parse_error;
	if (!(timeyear = misc_digtoken(&ar, " \t\r\n"))) goto parse_error;

	if ((strlen(month) + strlen(day) + strlen(timeyear)) <= 10) 
		snprintf(date, sizeof(date), "%s %s %s", month, day, timeyear);
	else
		strcpy(date, "Jan 1 1970");

	//strncpy(date, ar, 12);
	//date[12] = 0;
	//if (strlen(date) != 12) 
	//	goto parse_error;

	// Now, there HAS to be ONE space after the date.
	// Anymore and it COULD be part of the filename.


	// The rest is file/dir name.
	// This is faulty. Technically would parse filenames starting with
	// a space incorrectly. " hello" would be "hello".
	
	//name = &ar[12];
	name = ar;

	while (*name == ' ') name++; // <- wrong
	

#if 0
	printf("Parsed line: \n");
	printf("\tperm:    '%s'\n", perm);
	printf("\tthingy:  '%s'\n", thingy);
	printf("\tuser:    '%s'\n", user);
	printf("\tgroup:   '%s'\n", group);
	printf("\tsize:    '%s'\n", size);
	printf("\tdate:    '%s'\n", date);
	printf("\tname:    '%s'\n", name);
#endif
	

	// Check it is either a file or a dir. Ignore all other
	
	switch (tolower(perm[0])) {

	case 'd':
		directory = YNA_YES;
		break;

	case '-':
		directory = YNA_NO;
		break;

	case 'l':
        // Ok we need to handle links as well
        directory = YNA_AUTO;
        soft_link = YNA_YES;
		break;

	default:
		// Skip all others
		goto parse_error;
	}



	// Always ignore . and ..
	// really? 
	if (!strcmp(name, ".") || !strcmp(name, "..")) {
		file_free(result);
		return NULL;
	}

	result->size = (lion64u_t) strtoull(size, NULL, 10);
	
	result->directory = directory;
	result->soft_link = soft_link;

	result->name = strdup(name);

	result->user = strdup(user);
	result->group = strdup(group);
	result->date = misc_getdate(date);
	strcpy(result->perm, perm);


	//debugf("[file] parsed '%s' ok\n", name);

	// Return node.
	return result;

 parse_error:
	debugf("[file] unable to parse input: '%s'\n", ar ? ar : "(null)");
	file_free(result);
	return NULL;

}


void file_free_static(file_t *file)
{
	
	if (!file) return;

	SAFE_FREE(file->name);
	SAFE_FREE(file->fullpath);
	SAFE_FREE(file->dirname);
	SAFE_FREE(file->user);
	SAFE_FREE(file->group);
	SAFE_FREE(file->raw);

}

void file_free(file_t *file)
{
	
	if (!file) return;

	file_free_static(file);

	SAFE_FREE(file);

}




//
// If "fullpath" is set, then we trust it is correct and fill in the
// "name" and "dirname" sections.
//
// If "fullpath" is not set, we combine "name" and "dirname" to be "fullpath".
//
void file_makepath(file_t *file, char *cwd)
{
	char *r;

	if (file->fullpath) {
		// Create name and dirname.

		r = strrchr(file->fullpath, '/');

		if (!r) { // Shouldn't happen as fullpath should be _full path_
			SAFE_COPY(file->dirname, "/");
			SAFE_COPY(file->name, file->fullpath);
			return;
		}

		*r = 0;
		if (*file->fullpath) {
			SAFE_COPY(file->dirname, file->fullpath);
		} else {
			SAFE_COPY(file->dirname, "/");
		}

		SAFE_COPY(file->name, &r[1]);

		*r = '/';
		return;
	}

	// fullpath not set.
	if (cwd && file->name) {
		SAFE_FREE(file->fullpath);
		file->fullpath = misc_strjoin(cwd, file->name);
		SAFE_COPY(file->dirname, cwd);
	} else if (file->dirname && file->name) {
		SAFE_FREE(file->fullpath);
		file->fullpath = misc_strjoin(file->dirname, file->name);
	} else {
		debugf("[file] can't file_makepath\n");
	}
}




void file_dupe(file_t *dst, file_t *src)
{

	SAFE_COPY(dst->name,     src->name);
	SAFE_COPY(dst->fullpath, src->fullpath);
	SAFE_COPY(dst->dirname,  src->dirname);
	SAFE_COPY(dst->user,     src->user);
	SAFE_COPY(dst->group,    src->group);

	SAFE_COPY(dst->raw,      src->raw);

	memcpy(&dst->perm, src->perm, sizeof(dst->perm));

	dst->date      = src->date;
	dst->size      = src->size;
	dst->rest      = src->rest;
	dst->directory = src->directory;
	dst->soft_link = src->soft_link;

}



//
// Take src-file node AND
// dst-file node, to populate the dst-file node with
// appropriate information.
//
// If dst "fullpath" is set, we trust it. if not
// copy over the "name" from source, and build from that.
//
void file_makedest(file_t *dst, file_t *src, char *cwd)
{

	if (!dst->fullpath && !dst->name)
		SAFE_COPY(dst->name, src->name);

	file_makepath(dst, cwd);

	dst->directory = src->directory;
	dst->soft_link = src->soft_link;

}




//
// Take a slash-seperated list string, and a filename and
// attempt to match it.
//
// "*.bin/*ENGLISH*" "somefile.zip"
//
int file_listmatch(char *list, char *name)
{
	char *ar, *ext;
	

	// Start of the "list"
	ar = list;

	// While we pull out a token
	while((ext = misc_digtoken(&ar, "/"))) {
		
		
		// Does it match?
                        
		if (!lfnmatch(ext,
					  name, 
					  LFNM_CASEFOLD)) {

			//if ((ar > list))
			  if (misc_digtoken_optchar)
	//			ar[-1] = misc_digtoken_optchar;
		list[ strlen(list) ] =misc_digtoken_optchar;
	return 1;
		}

//		if ((ar > list))
	//		ar[-1] = misc_digtoken_optchar;
		list[ strlen(list) ] =misc_digtoken_optchar;
	}

	return 0;

}
