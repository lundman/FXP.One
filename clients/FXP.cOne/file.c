// $Id: file.c,v 1.3 2005/07/28 09:08:25 lundman Exp $
// 
// Jorgen Lundman 18th March 2004.
//
// 
//


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "misc.h"

#include "file.h"



file_t *file_new(void)
{
        file_t *result;

        result = (file_t *) calloc(sizeof(*result), 1);

        return result;

}



void file_free(file_t *file)
{

        SAFE_FREE(file->display_name);
        SAFE_FREE(file->name);
        SAFE_FREE(file->user);
        SAFE_FREE(file->group);
        SAFE_FREE(file->perm);

        SAFE_FREE(file);

}



void file_strdup_name(file_t *file, char *name, char *type)
{
	char buffer[1024];
	char *test = NULL;
	
	file->name = misc_url_decode(name);

	if (!mystrccmp(type, "DIRECTORY")) {
		file->directory = YNA_YES;
		snprintf(buffer, sizeof(buffer), "</8>%s/<!8>", file->name);
		file->display_name = strdup(buffer);
		return;
	}

	file->directory = YNA_NO;

	snprintf(buffer, sizeof(buffer), "</32>%s<!32>", test ? test : file->name);

	file->display_name = strdup(buffer);

	return;

}
