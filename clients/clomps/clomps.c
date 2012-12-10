
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef WIN32
#include <unistd.h>
#include <sys/time.h>
#endif

#include "lion.h"
#include "misc.h"

#include "clomps.h"
#include "debug.h"

#include "file.h"
#include "site.h"
#include "conf.h"
#include "autoq.h"


static fxpone_t *fxpone = NULL;


int do_exit = 0;
int connected = 0;






int lion_userinput(lion_t *handle, void *user_data,
	int status, int size, char *line){return 0;}



fxpone_t *fxpone_newnode(void)
{
	fxpone_t *result;

	if (fxpone) {
		debugf("error: currently we only support conencting to ONE FXP.One engine.\n");
		return NULL;
	}


	result = malloc(sizeof(*result));
	if (!result) return NULL;

	memset(result, 0, sizeof(*result));

	fxpone = result;

	return result;
}


void fxpone_freenode(fxpone_t *node)
{

	if (node->handle) {
		lion_t *handle;

		handle = node->handle;
		node->handle = NULL;
		lion_set_userdata(handle, NULL);
		lion_close(handle);

	}

	SAFE_FREE(node->host);
	SAFE_FREE(node->user);
	SAFE_FREE(node->pass);

	SAFE_FREE(node);
}





#ifdef WIN32
/* Windows implementation by Martin Lambers <address@hidden>,
   improved by Simon Josefsson. */

/* For PASS_MAX. */
#include <limits.h>

#ifndef PASS_MAX
# define PASS_MAX 512
#endif

char *
getpass (const char *prompt)
{
  char getpassbuf[PASS_MAX + 1];
  size_t i = 0;
  int c;

  if (prompt)
    {
      fputs (prompt, stderr);
      fflush (stderr);
    }

  for (;;)
    {
      c = _getch ();
      if (c == '\r')
       {
         getpassbuf[i] = '\0';
         break;
       }
      else if (i < PASS_MAX)
       {
         getpassbuf[i++] = c;
       }

      if (i >= PASS_MAX)
       {
         getpassbuf[i] = '\0';
         break;
       }
    }

  if (prompt)
    {
      fputs ("\r\n", stderr);
      fflush (stderr);
    }

  return strdup (getpassbuf);
}
#endif



void exit_interrupt(void)
{
	do_exit = 1;
}


int main(int argc, char **argv)
{
	char buffer[1024];

	memset(&fxpone, 0, sizeof(fxpone));

	arguments(argc, argv);

	if (lion_init()) {
		exit(0);
	}

	conf_read(NULL);

	if (!fxpone) {
		printf("No FXPONE entry in .conf file error.\n");
		exit(1);
	}

	if (!fxpone->user) {
		printf("Username: ");
		fflush(stdout);
		fgets(buffer, 1024, stdin);
		SAFE_COPY(fxpone->user, buffer);
	}

	if (!fxpone->pass) {
		char *pass;
		pass = getpass("Password: ");
		SAFE_COPY(fxpone->pass, pass);
	}


	// COnnect to FXP.One
	fxpone->handle = lion_connect(fxpone->host,
								  fxpone->port, 0, 0, LION_FLAG_FULFILL,
								  fxpone);

	lion_set_handler(fxpone->handle, site_handler);


	while(!do_exit && (num_done < num_sites)) {

		lion_poll(0, 5);

	}


	if (num_done == num_sites) {

		sites_compare();
        do_exit = 2;
	}


	if (conf_do_autoq) {
		do_exit = 0;
		//		autoq_process(fxpone);
	}

	while(!do_exit) {

		lion_poll(0, 1);
		autoq_check(fxpone);

	}

    printf("Saving updated time-stamps...\n");
    if (do_exit == 2) site_savecfg();

	printf("All done.\n");


	if (fxpone->handle)
		lion_printf(fxpone->handle, "QUIT\r\n");

	fxpone_freenode(fxpone);
	fxpone = NULL;

	lion_free();

	return 0;
}













