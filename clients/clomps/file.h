#ifndef FILE_INCLUDED_H
#define FILE_INCLUDED_H

#ifndef WIN32
#include <sys/time.h>
#endif

#include "lion_types.h"

struct file_struct {
	char *name;
	time_t date;
	lion64_t size;
	int type;
	char *status;
	unsigned int current_dir;
    int queued;    // If we've sent QADD already
    // if any part of a file was transferred, increase me, this is for re-queueing
    // until complete logic.
    int transfers;

};

typedef struct file_struct file_t;


file_t    *file_new   ( void );
void       file_free  ( file_t * );
file_t    *file_dupe  ( file_t * );


int sort_by_date(const void *a, const void *b);
int file_listmatch(char *list, char *name);
file_t *file_find(file_t **files, unsigned int num, char *name);

#endif
