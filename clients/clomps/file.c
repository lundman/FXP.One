#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "misc.h"
#include "file.h"
#include "lfnmatch.h"

file_t *file_new(void)
{
	file_t *result;

	result = (file_t *) malloc(sizeof(*result));

	if (result) memset(result, 0, sizeof(*result));

	return result;

}


void file_free(file_t *file)
{

	SAFE_FREE(file->name);

	SAFE_FREE(file);

}

file_t *file_dupe(file_t *old)
{
	file_t *result;

	result = file_new();
	if (!result) return NULL;

	SAFE_COPY(result->name, old->name);
	result->date = old->date;
	result->size = old->size;
	result->type = old->type;

	return result;
}









int sort_by_date(const void *a, const void *b)
{
	time_t aa = (*(file_t **) a)->date;
	time_t bb = (*(file_t **) b)->date;

	if (aa > bb) return -1;
	if (aa < bb) return 1;
	return 0;
}






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

            list[ strlen(list) ] =misc_digtoken_optchar;
			//if ((ar > list))
            //ar[-1] = misc_digtoken_optchar;
			return 1;
		}

        list[ strlen(list) ] =misc_digtoken_optchar;
		//if ((ar > list))
        //ar[-1] = misc_digtoken_optchar;

	}

	return 0;

}





file_t *file_find(file_t **files, unsigned int num, char *name)
{
	int j;

	for (j = 0; j < num; j++)
		if (files[j] && !mystrccmp(files[j]->name, name)) return files[j];

	return NULL;
}

