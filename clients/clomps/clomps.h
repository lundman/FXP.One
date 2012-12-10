#ifndef CLOMPS_INCLUDED_H
#define CLOMPS_INCLUDED_H


extern int do_exit;
extern int connected;



struct fxpone_struct {
	char *host;
	int port;
	char *user;
	char *pass;
	int ssl;
	lion_t *handle;
};

typedef struct fxpone_struct fxpone_t;


fxpone_t *fxpone_newnode(void);
void fxpone_freenode(fxpone_t *node);



#endif
