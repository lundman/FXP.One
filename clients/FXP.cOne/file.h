// $Id: file.h,v 1.1 2005/07/04 01:08:25 lundman Exp $
// 
// Jorgen Lundman 18th March 2004.

#ifndef FILE_H_INCLUDED
#define FILE_H_INCLUDED


#include "lion_types.h"
#include <sys/types.h>


// Defines








// Variables

enum yesnoauto_enum {
	YNA_NO = 0,
	YNA_YES,
	YNA_AUTO
};

typedef enum yesnoauto_enum yesnoauto_t;



struct file_struct {

	char *name;
	char *display_name;
	time_t date;
	lion64u_t size;
	char *user;
	char *group;
	char *perm;

	yesnoauto_t directory;
	yesnoauto_t soft_link;
	yesnoauto_t queued;

	unsigned int fid;
};

typedef struct file_struct file_t;







// Functions

file_t      *file_new        ( void );
void         file_free       ( file_t * );
void         file_strdup_name( file_t *, char *, char * );



#endif
