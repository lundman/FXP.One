// $Id: file.h,v 1.1 2004/02/23 01:27:30 lundman Exp $
// 
// Jorgen Lundman 18th March 2004.

#ifndef FILE_H_INCLUDED
#define FILE_H_INCLUDED


#include "lion_types.h"
#include <sys/types.h>
#include "settings.h"

// Defines








// Variables

// Warning, remember to reflect changed here in file_dupe function.
struct file_struct {

	unsigned int fid;

	char *name;      // Just filename
	char *fullpath;  // The full path dirname + filename
	char *dirname;   // Just the dirname minus filename
	time_t date;
	lion64u_t size;
	lion64u_t rest;
	char *user;
	char *group;
	char perm[14];

	yesnoauto_t directory;
	yesnoauto_t soft_link;

	char *raw;

};

typedef struct file_struct file_t;







// Functions

file_t      *file_parse      ( char *, int );
void         file_free       ( file_t * );
void         file_free_static( file_t * );
void         file_makepath   ( file_t *, char * );
void         file_dupe       ( file_t *, file_t * );
void         file_makedest   ( file_t *, file_t *, char * );
int          file_listmatch  ( char *, char * );
void         file_strdup_name( file_t *, char *, char * );



#endif
