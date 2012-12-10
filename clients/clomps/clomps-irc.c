
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
#include "irc.h"
#include "autoq.h"


static fxpone_t    *fxpone      = NULL;


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
    int i;

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


    // Attempt to (re-)connect to engine.
    do {

        // Connect to FXP.One
        connected = 1;
        fxpone->handle = lion_connect(fxpone->host,
                                      fxpone->port, 0, 0, LION_FLAG_FULFILL,
                                      fxpone);

        lion_set_handler(fxpone->handle, site_handler);

        irc_init();

        // Set all sites to skip, so that the fxp.one engine thinker does not
        // connect and list all sites.
        for (i = 0; i < num_sites; i++) {
            if (!sites[i]) continue;
            sites[i]->skip = 1;
        }

        while(!do_exit && connected) {

            lion_poll(0, 5);

        }

        if (!do_exit) sleep(30); // Don't reconnect too often

    } while(!do_exit);

	printf("\nAll done.\n");

	if (fxpone->handle)
		lion_printf(fxpone->handle, "QUIT\r\n");

    irc_free();

	fxpone_freenode(fxpone);
	fxpone = NULL;

	lion_free();

	return 0;
}





void clomps_ircqueue(trade_t *trade, char *releasename,
                     ircserver_t *ircserver, char *channel,
                     char *from)
{
    site_t *src, *dst;
    file_t **tmp, *new_file;
    autoq_t *aq;
    char *r;

    // Check that it matches Accept.
    if (!file_listmatch(trade->accept, releasename))
        return;

    // Check is passes the Reject list.
    if (trade->reject && file_listmatch(trade->reject, releasename))
        return;

    // Attempt to ascertain if the release is top level, or a subdir. Check
    // if subdir moves are wanted.
    if (!trade->subdir) {
        r = strchr(releasename, '/');
        if (r) {
            while(*r == '/') r++; // Skip all slashes
            if (strlen(r) > 0) {  // Is subdir
                debugf("[irc] subdir detected on '%s'. cmd SUBDIR not in use, skipping\n",
                       releasename);
                return;
            }
        }
    }

    debugf("[irc] QUEUE: %s:%s/%s ==>> %s:%s/%s (Trigger: %s:%d/%s <%s>)\n",
           trade->srcsite, trade->srcdir, releasename,
           trade->dstsite, trade->dstdir, releasename,
           ircserver->server, ircserver->port,
           channel, from);


    // If there is an autoq, but it has already finished, release it
    if (trade->aq && (trade->aq->qid <= 0)) {
        autoq_freenode(trade->aq);
        trade->aq = NULL;
    }

    // If we don't have an autoq defined, create one.
    if (!trade->aq) {

		src = site_find(trade->srcsite);
		dst = site_find(trade->dstsite);
        if (!src || !dst) return;

        aq = autoq_newnode();
        if (!aq) return;

        trade->aq = aq;

        // Assign over site details
        aq->fxpone   = fxpone;
        aq->from     = strdup(trade->srcsite);
        aq->to       = strdup(trade->dstsite);
		aq->src_site = src;
		aq->dst_site = dst;



    } else {

        // Add to the existing queue
        aq = trade->aq;

    }


    new_file = file_new();
    if (!new_file) return;

    new_file->name = strdup(releasename);
    // Is it always directories?
    new_file->type = 1;

    // This entry should be queued.
    tmp = realloc(aq->files,
                      sizeof(file_t *)*(aq->num_files + 1));
    if (!tmp) return;

    aq->requeue = trade->requeue;
    aq->files = tmp;
    aq->files[ aq->num_files ] = new_file;
    aq->num_files++;

    if (conf_do_verbose)
        printf(" %s->%s: queued: %s\n",
               aq->from,
               aq->to,
                   new_file->name);

    // Locate SRCDIR in src_site->dirs[] and set current_dir correctly
    // ditto DST
    aq->src_site->current_dir = 0;
    aq->dst_site->current_dir = 0;

    debugf("[irc] calling autoq ... \n");
    autoq_start(aq);
}
