#ifndef AUTOQ_INCLUDED_H
#define AUTOQ_INCLUDED_H

enum autoq_enum {
	AUTOQ_CONNECTED_NONE = 0,
	AUTOQ_CONNECTED_ONE = 1,
	AUTOQ_CONNECTED_BOTH = 2,

	// Anything less than COMPLETE is considered "in progress".
	AUTOQ_COMPLETE = 3,
	AUTOQ_FAILED
};

typedef enum autoq_enum autoq_status_t;

struct autoq_struct {
	unsigned int passnum;

	char *from;
	site_t *src_site;

	char *to;
	site_t *dst_site;

	unsigned int qid;

	char *accept;
	char *reject;

	char *start_path;

	unsigned int num_files;
	file_t **files;

	fxpone_t *fxpone;

	autoq_status_t status;

    int incskip;
    int requeue;
    int running;

	struct autoq_struct *next;
};

typedef struct autoq_struct autoq_t;






void     autoq_process    ( fxpone_t * );
void     autoq_add        ( char *, char *, char *, char *, char *, char *, char *);
void     autoq_assign_sid ( unsigned int, char * );
autoq_t *autoq_find_by_sid( unsigned int );
void     autoq_check      ( fxpone_t * );
autoq_t *autoq_find_by_qid( unsigned int );
autoq_t *autoq_newnode    ( void );
void     autoq_freenode   ( autoq_t *aq );
void     autoq_start      ( autoq_t *aq );

#endif
