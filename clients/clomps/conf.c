#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#ifndef WIN32
#include <getopt.h>
#include <unistd.h>
#endif

#ifdef WIN32
extern int getopt();
extern char *optarg;
extern int optind;
#endif


#include "lion.h"
#include "misc.h"
#include "parser.h"
#include "conf.h"
#include "clomps.h"
#include "file.h"
#include "site.h"
#include "autoq.h"
#include "irc.h"
#include "debug.h"

char    *conf_file_name  = NULL;
char    *conf_incpattern = NULL;
char    *conf_autoqpattern = NULL;
time_t   default_age     = 0;
int      conf_do_autoq   = 0;
int      conf_do_save    = 1;
int      conf_do_verbose = 0;


static parser_command_list_t conf_command_list[] = {
        { COMMAND( "FXPONE"    ),   conf_fxpone    },
        { COMMAND( "SITE"      ),   conf_site      },
        { COMMAND( "TIMESTAMP" ),   conf_timestamp },
        { COMMAND( "AUTOQ"     ),   conf_autoq     },
        { COMMAND( "CONTINUED" ),   conf_continued },
        { COMMAND( "IRC"       ),   conf_irc       },
        { COMMAND( "TRADE"     ),   conf_trade     },
        { NULL,  0              ,         NULL     }
};





void options(char *prog)
{

        printf("\n");
        printf("%s - FXP.One client compare.\n", prog);
        printf("%s [-hiqn] [...]\n\n", prog);
        printf("  options:\n");

        printf("  -h          : display usage help (this output)\n");
        printf("  -f <file>   : specify conf filename. Default clomps.conf\n");
        printf("  -a <sec>    : subtract <sec> from stored last_check time\n");
		printf("  -i <pattern>: search for pattern. '*' would show all as new\n");
		printf("  -I <pattern>: AUTOQ pattern. Always consider pattern for autoq\n");
		printf("  -q          : process AUTOQ commands after comparison\n");
		printf("  -n          : do not save updated timestamps to conf file\n");
		printf("  -v          : verbose output\n");
        printf("\n\n(c) Jorgen Lundman <lundman@lundman.net>\n\n");

        exit(0);

}



void arguments(int argc, char **argv)
{
	int opt;

	while ((opt=getopt(argc, argv,
					   "hf:a:i:I:qnvd")) != -1) {

		switch(opt) {

		case 'h':
			options(argv[0]);
			break;

		case 'f':
			SAFE_COPY(conf_file_name, optarg);
			break;

		case 'a':
			default_age = strtoul(optarg, NULL, 10);
			break;

		case 'i':
			SAFE_COPY(conf_incpattern, optarg);
			break;

		case 'I':
			SAFE_COPY(conf_autoqpattern, optarg);
			break;

		case 'q':
			conf_do_autoq ^= 1;
			break;

		case 'n':
			conf_do_save ^= 1;
			break;

		case 'v':
			conf_do_verbose ^= 1;
			break;

		case 'd':
			debug_on ^= 1;
			break;

		default:
			printf("Unknown option.\n");
			break;
		}
	}

	argc -= optind;
	argv += optind;

	// argc and argv adjusted here.
	if (!conf_file_name)
		conf_file_name = strdup("clomps.conf");
}




// FXPONE|HOST=127.0.0.1|PORT=8885|USER=admin|SSL=forced|TIMEFILE=name
void conf_fxpone(char **keys, char **values, int items,void *optarg)
{
	char *host, *port, *user, *pass, *ssl, *timefile;
	fxpone_t *fxpone;

	host     = parser_findkey(keys, values, items, "HOST");
	port     = parser_findkey(keys, values, items, "PORT");
	user     = parser_findkey(keys, values, items, "USER");
	pass     = parser_findkey(keys, values, items, "PASS");
	ssl      = parser_findkey(keys, values, items, "SSL");
	timefile = parser_findkey(keys, values, items, "TIMEFILE");

	if (!host) {
		printf("FXPONE Entry in conf without HOST= field\n");
		return;
	}

    // Don't need to save anymore
#if 0
	if (optarg) {
		// Saving
		lion_printf(optarg, "FXPONE|HOST=%s", host);
		if (port) lion_printf(optarg, "|PORT=%s", port);
		if (user) lion_printf(optarg, "|USER=%s", user);
		if (pass) lion_printf(optarg, "|PASS=%s", pass);
		if (ssl)  lion_printf(optarg, "|ssl=%s",  ssl);
		lion_printf(optarg, "\r\n");
		return;
	}
#endif

	// Create a new fxpone engine to connect to.
	fxpone = fxpone_newnode();
	if (!fxpone) return;


	SAFE_COPY(fxpone->host, host);
	SAFE_COPY(fxpone->user, user);
	SAFE_COPY(fxpone->pass, pass);
	if (port)
		fxpone->port = atoi(port);
	else
		fxpone->port = 8885;

	if (ssl)
		fxpone->ssl = atoi(ssl);

    SAFE_COPY(fxpone->timefile, timefile);
}


//SITE|NAME=foo|DIR=/tv/|USESKIP=1|INCTEST=%s-INCOMPLETE|LAST_CHECK=123123123
//NUKETEST=!NUKED-%s
void conf_site(char **keys, char **values, int items,void *optarg)
{
	char *name, *dir, *useskip, *inctest, *nuketest, *last_check, *hide;
	site_t *site, **tmp;
	char **dtmp;
    char *strtmp;
    int i;

	name       = parser_findkey(keys, values, items, "NAME");
	useskip    = parser_findkey(keys, values, items, "USESKIP");
	inctest    = parser_findkey(keys, values, items, "INCTEST");
	nuketest   = parser_findkey(keys, values, items, "NUKETEST");
	hide       = parser_findkey(keys, values, items, "HIDE");
	last_check = parser_findkey(keys, values, items, "LAST_CHECK");

	if (!name) {
		printf("SITE Entry in conf without NAME= field\n");
		return;
	}

    // Attempt to find a site already defined by name, if found, allow
    // HIDE to be concatenated.
    for (i = 0; i < num_sites; i++) {
        if (sites[i] && !mystrccmp(name, sites[i]->name)) {
            if (hide) {
                if (!sites[i]->hide) {
                    SAFE_COPY(sites[i]->hide, hide);
                } else {
                    strtmp = sites[i]->hide;
                    sites[i]->hide = misc_strjoin(strtmp?strtmp:"", hide);
                    SAFE_FREE(strtmp);
                }
            }
            return;
        }
    }

	site = calloc(1, sizeof(*site));
	if (!site) return;

	tmp = realloc(sites, sizeof(site_t *) * (num_sites + 1));
	if (!tmp) return;
	sites = tmp;

	sites[num_sites] = site;
	num_sites++;

	SAFE_COPY(site->name, name);

	while ((dir = parser_findkey_once(keys, values, items, "DIR"))) {
		dtmp = (char **) realloc(site->dirs,
								 sizeof(char *) * (site->num_dirs + 1));
		if (!dtmp) break;

		site->dirs = dtmp;

		site->dirs[ site->num_dirs ] = strdup(dir);
		site->num_dirs++;

	}

	SAFE_COPY(site->inctest, inctest);
	SAFE_COPY(site->nuketest, nuketest);
	SAFE_COPY(site->hide, hide);
	if (useskip)
		site->use_lists = atoi(useskip);
	if (last_check) {
		site->last_check = strtoul(last_check, NULL, 10);
		site->last_check -= default_age;
	} else {
		site->last_check = lion_global_time;
		site->last_check -= default_age;
	}


    site->num_files = 0;
}

//TIMESTAMP|NAME=foo|LAST_CHECK=123123123
void conf_timestamp(char **keys, char **values, int items,void *optarg)
{
	char *name, *last_check;
	site_t *site = NULL;
    int i;

	name       = parser_findkey(keys, values, items, "NAME");
	last_check = parser_findkey(keys, values, items, "LAST_CHECK");

	if (!name) {
		printf("TIMESTAMP Entry in conf without NAME= field\n");
		return;
	}

    for (i = 0; i < num_sites; i++) {
        if (sites[i] && !mystrccmp(name, sites[i]->name)) {
            site = sites[i];
            break;
        }
    }

    if (!site) {
        printf("Unable to find site '%s' used in TIMESTAMP\n", name);
        return;
    }

	if (last_check) {
		site->last_check = strtoul(last_check, NULL, 10);
		site->last_check -= default_age;
	} else {
		site->last_check = lion_global_time;
		site->last_check -= default_age;
	}

}



//AUTOQ|PASSNUM=x|FROM=site|TO=site|ACCEPT=p1/p2/...|REJECT=p1/p2/...
void conf_autoq(char **keys, char **values, int items,void *optarg)
{
	char *passnum, *from, *to, *accept, *reject, *incskip, *requeue;

	passnum    = parser_findkey(keys, values, items, "PASSNUM");
	from       = parser_findkey(keys, values, items, "FROM");
	to         = parser_findkey(keys, values, items, "TO");
	accept     = parser_findkey(keys, values, items, "ACCEPT");
	reject     = parser_findkey(keys, values, items, "REJECT");
	incskip    = parser_findkey(keys, values, items, "INCSKIP");
	requeue    = parser_findkey(keys, values, items, "REQUEUE");

	if (!passnum || !from || !to) {
		printf("AUTOQ Entry in conf without PASSNUM, FROM, TO fields.\n");
		return;
	}

	autoq_add(passnum, from, to, accept, reject, incskip, requeue);

}

//CONTINUED|ACCEPT=p1/p2/...|REJECT=p1/p2/...
void conf_continued(char **keys, char **values, int items,void *optarg)
{
	char *accept, *reject;

	accept     = parser_findkey(keys, values, items, "ACCEPT");
	reject     = parser_findkey(keys, values, items, "REJECT");

	autoq_add(NULL, NULL, NULL, accept, reject, NULL, NULL);

}


//IRC|server=irc|port=6667|pass=ircpass|nick=clomps|user=|ssl=yes
//   |channel=#bots|channel=#another

void conf_irc(char **keys, char **values, int items,void *optarg)
{
	char *server, *nick, *user, *port, *pass, *ssl, *channel;
    ircserver_t *ircserver = NULL;

    // Mandatory
	server  = parser_findkey(keys, values, items, "SERVER");
	nick    = parser_findkey(keys, values, items, "NICK");
	user    = parser_findkey(keys, values, items, "USER");

	if (!server || !nick || !user) {
		printf("IRC Entry in conf without SERVER, NICK or USER fields.\n");
		return;
	}

	port    = parser_findkey(keys, values, items, "PORT");
	pass    = parser_findkey(keys, values, items, "PASS");
	ssl     = parser_findkey(keys, values, items, "SSL");

    // Pull out the channel defines.
    while((	channel  = parser_findkey_once(keys, values, items, "CHANNEL"))) {

        if (!ircserver) {
            ircserver = irc_addserver(server, port, pass, nick, user, ssl);
        }
        if (ircserver) {
            irc_addchannel(ircserver, channel);
        }

    }

}


// TRADE | NICK=clomps | MATCH=botname.*New Release (.*)
//   | SRCSITE=ASite1     | SRCDIR=/tv
//   | DSTSITE=glftpdsite | DSTDIR=/tv
//   | ACCEPT=*simpsons* | REJECT=*FINSUB*
void conf_trade(char **keys, char **values, int items,void *optarg)
{
    char *nick, *match, *srcsite, *srcdir, *dstsite, *dstdir, *accept, *reject;
    char *requeue, *subdir;

	nick    = parser_findkey(keys, values, items, "NICK");
	match   = parser_findkey(keys, values, items, "MATCH");
	srcsite = parser_findkey(keys, values, items, "SRCSITE");
	srcdir  = parser_findkey(keys, values, items, "SRCDIR");
	dstsite = parser_findkey(keys, values, items, "DSTSITE");
	dstdir  = parser_findkey(keys, values, items, "DSTDIR");
	accept  = parser_findkey(keys, values, items, "ACCEPT");
	reject  = parser_findkey(keys, values, items, "REJECT");

    if (!nick || !match || !srcsite || !srcdir || !dstsite || !dstdir ||
        !accept || !reject) {
		printf("TRADE Entry in conf without NICK, MATCH, SRCSITE, SRCDIR, DSTSITE, DSTDIR, ACCEPT or REJECT\n");
		return;
	}

    debugf("MATCH '%s'\n", match);

	requeue = parser_findkey(keys, values, items, "REQUEUE");
	subdir  = parser_findkey(keys, values, items, "SUBDIR");

    irc_add_trade(nick, match, srcsite, srcdir, dstsite, dstdir, accept, reject,
                  requeue ? 1 : 0,
                  subdir ? 1 : 0);

}





int conf_file_handler(lion_t *handle, void *user_data,
					  int status, int size, char *line)
{

	switch(status) {

	case LION_FILE_OPEN:
		break;


	case LION_FILE_FAILED:
	case LION_FILE_CLOSED:
		do_exit = 1;
		break;

	case LION_INPUT:
		//printf("settings: read '%s'\n", line);

		parser_command(conf_command_list, line, user_data);

		break;

	}

	return 0;

}







void conf_read(void)
{
	lion_t *conf_file;

	conf_file = lion_open(conf_file_name, O_RDONLY, 0600,
						  LION_FLAG_NONE, NULL);

	if (!conf_file) {
		perror("open:");
		exit(1);
	}

	lion_set_handler(conf_file, conf_file_handler);

	do_exit = 0;
	while(!do_exit)
		lion_poll(0,1);
	do_exit = 0;

}

void conf_time(char *timefile)
{
	lion_t *conf_file = NULL;
    char *tmp, *r;

    if (!timefile) {
        tmp = misc_strjoin(conf_file_name, "timestamp");
        if ((r = strrchr(tmp, '/'))) *r = '.';
        conf_file = lion_open(tmp, O_RDONLY, 0600,
                              LION_FLAG_NONE, NULL);
        SAFE_FREE(tmp);
    } else {
        conf_file = lion_open(timefile, O_RDONLY, 0600,
                              LION_FLAG_NONE, NULL);
    }

    // Not having a timestamp file is OK
	if (!conf_file) {
        return;
	}

	lion_set_handler(conf_file, conf_file_handler);

	do_exit = 0;
	while(!do_exit)
		lion_poll(0,1);
	do_exit = 0;

}

void conf_save(lion_t *savefile)
{
    int i;

    for (i = 0; i < num_sites; i++) {
        if (sites[i]) {
            lion_printf(savefile, "TIMESTAMP|NAME=%s", sites[i]->name);
            lion_printf(savefile, "|LAST_CHECK=%lu", sites[i]->last_check);
            lion_printf(savefile, "\r\n");
        }
    }
}
