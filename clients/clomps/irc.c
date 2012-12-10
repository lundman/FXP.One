#if HAVE_CONFIG_H
#include "config.h"
#endif

#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "lion.h"
#include "misc.h"

#include "clomps.h"
#include "debug.h"

#include "file.h"
#include "site.h"
#include "conf.h"
#include "irc.h"
#include "autoq.h"

#if HAVE_PCRE && CLOMPS_IRCc
#include <pcre.h>
#else
#include "lfnmatch.h"
#ifndef WIN32
#warning
#warning "clomps-irc require libpcre for full functionality"
#warning
#endif
#endif

static ircserver_t *irc_servers = NULL;
static trade_t     *irc_trades  = NULL;

ircserver_t *irc_addserver(char *strserver, char *port, char *pass, char *nick,
                           char *user, char *ssl)
{
    ircserver_t *server;

    if (!strserver || !nick || !user) return NULL;

    server = calloc(sizeof(*server), 1);
    if (!server) return NULL;

    SAFE_COPY(server->server, strserver);
    SAFE_COPY(server->pass,   pass);
    SAFE_COPY(server->nick,   nick);
    SAFE_COPY(server->user,   user);

    if (ssl && *ssl) {
        if (!mystrccmp(ssl, "yes") ||
            !mystrccmp(ssl, "on") ||
            !mystrccmp(ssl, "forced"))
            server->ssl = 1;
    }

    if (port && *port)
        server->port = atoi(port);

    debugf("[irc] Adding new server '%s:%d'\n", server->server, server->port);

    // Link-in
    server->next = irc_servers;
    irc_servers = server;

    return server;
}



void irc_addchannel(ircserver_t *ircserver, char *strchannel)
{
    ircchannel_t *channel;

    if (!ircserver || !strchannel) return;

    channel = calloc(sizeof(*channel), 1);
    if (!channel) return;

    debugf("[irc]     adding channel '%s'\n", strchannel);
    SAFE_COPY(channel->channel, strchannel);

    // Link-in
    channel->next = ircserver->channels;
    ircserver->channels = channel;

}


int irc_init(void)
{
    ircserver_t *server;
    time_t now;
    // Connect all IRC Servers.

    time(&now);

    for (server = irc_servers;
         server;
         server=server->next) {

        if (server->handle) continue; // Already connected

        // If not set yet, make default sleep 30s;
        if (!server->sleep)
            server->sleep = 30;

        // Don't connect too often, never connected,
        // or, "sleep" number of seconds has passed;
        if (!server->last_time ||
            ((server->last_time + server->sleep) < now)) {

            // Fire off connection
            server->last_time = now;

            server->handle = lion_connect(server->server, server->port,
                                          0, 0, LION_FLAG_FULFILL, (void *)server);
            lion_ssl_set(server->handle, LION_SSL_CLIENT);
            lion_set_handler(server->handle,
                             irc_server_handler);

        } // if time to connect
    } // for all servers

    return 0;
}

void irc_free(void)
{
    ircserver_t *server, *next;
    ircchannel_t *channel, *chnext;
    trade_t *trade, *trnext;

    for (server = irc_servers;
         server;
         server=server->next) {

        if (!server->handle) continue; // Not connected

        lion_set_userdata(server->handle, NULL);
        lion_disconnect(server->handle);
        server->handle = NULL;

    }


    for (server = irc_servers;
         server;
         server=next) {
        next = server->next;

        SAFE_FREE(server->server);
        SAFE_FREE(server->pass);
        SAFE_FREE(server->nick);
        SAFE_FREE(server->user);

        for (channel = server->channels;
             channel;
             channel = chnext) {
            chnext = channel->next;

            SAFE_FREE(channel->channel);

            SAFE_FREE(channel);
        }

        SAFE_FREE(server);
    }


    for (trade = irc_trades; trade; trade=trnext) {
        trnext = trade->next;

        SAFE_FREE(trade->nick);
        SAFE_FREE(trade->match);
        SAFE_FREE(trade->srcsite);
        SAFE_FREE(trade->srcdir);
        SAFE_FREE(trade->dstsite);
        SAFE_FREE(trade->dstdir);
        SAFE_FREE(trade->accept);
        SAFE_FREE(trade->reject);

#if HAVE_PCRE && CLOMPS_IRC
        if (trade->re)
            pcre_free(trade->re);
        if (trade->nick_re)
            pcre_free(trade->nick_re);
#endif

        SAFE_FREE(trade);
    }

}

void irc_server_input(ircserver_t *server, char *line)
{
    unsigned int status = 0;
    char *ar, *token, *from, *r;
    ircchannel_t *channel;
    trade_t *trade;
    char *strchannel, *releasename = "";

    // If line starts with ":" parse out the status code

    ar = line;

    if (*line == ':') {

        // Fetch irc server name
        from = misc_digtoken(&ar, ": ");
        if (!from) return;

        // Fetch protocol number
        token = misc_digtoken(&ar, ": ");
        if (!token) return;

#ifdef DEBUG_VERBOSE
        printf("[irc_state] reply %d\n", atoi(token));
#endif

        // If it's "0" here, try to parse against other commands too...
        if (atoi(token)) status = atoi(token);

        // If from has a "!" truncate there, just the nick name
        if ((r = strchr(from, '!')))
            *r = 0;

        } else {

        // Doesn't start with ":" try to parse commands...
        // Fetch command
        token = misc_digtoken(&ar, ": ");
        if (!token) return;
    }

    if (status) { // Status code

        switch(status) {
        case 1:
            debugf("[irc] status connected.\n");
            //server->connected = 1;
            // Join all channels!
            for (channel = server->channels; channel; channel = channel->next) {
                if (!lion_printf(server->handle,
                                 "JOIN %s\r\n",
                                 channel->channel)) return;
            }
            break;
        case 433: // Nick in use!
            debugf("[irc] NICK collision\n");
            lion_disconnect(server->handle);
            return;
        }

        return;
    }

    // Command
    if (!mystrccmp("PING", token)) {
        lion_printf(server->handle, "PONG %s\r\n", ar);
        return;
    }

    // "ar" here is "#debug :text from user"
    strchannel = misc_digtoken(&ar, ": ");

    if (from && !mystrccmp("PRIVMSG", token)) {
        int match = 0;
        // From channel, or from user?
        debugf("[irc] PRIVMSG: <%s:%s> '%s'\n", from, strchannel, ar);


        // Look for trade matches

        for (trade = irc_trades; trade; trade=trade->next) {

#if HAVE_PCRE && CLOMPS_IRC
#define OVECOUNT 30
            {
                int ovector[OVECOUNT];

                // NICK name match
                match = pcre_exec(trade->nick_re,
                                  NULL,
                                  from,
                                  strlen(from),
                                  0,
                                  0,
                                  ovector,
                                  OVECOUNT);

                if (match < 0) continue; // NICK name do not match

                match = pcre_exec(trade->re,
                                  NULL,
                                  ar,
                                  strlen(ar),
                                  0,
                                  0,
                                  ovector,
                                  OVECOUNT);

                debugf("[irc] PCRE returned %d\n", match);
                // match == 0 is overflow on OVECOUNT
                if (match == 0) {
                    match = OVECOUNT / 3;
                }

                if (1) {
                    int i;
                    for (i = 0; i < match; i++)
                        debugf("    pcre: %d is %d : %d\n", i,
                           ovector[i*2], ovector[i*2+1]);
                }

                if (match >= 2) { // Get release name
                    ar[ovector[3]] = 0; // Terminate after the match
                    releasename = strdup(&ar[ovector[2]]);
                }
            }
#else
            // Does nick match
            match = lfnmatch(trade->nick,
                             from,
                             LFNM_CASEFOLD);
            if (match <= 0) continue; // NICK name does not match

            match = lfnmatch(trade->match,
                             ar,
                             LFNM_CASEFOLD);
            releasename = "*";  // Match all releases
#endif

            // No match
            if (match <= 0) continue;

            printf("[irc] MATCH '%s', releasename '%s'\n", trade->match,
                   releasename?releasename:"");

#if CLOMPS_IRC
            clomps_ircqueue(trade, releasename, server, strchannel, from);
#endif
            break;

        }

    }

}



int irc_server_handler(lion_t *handle, void *user_data,
                       int status, int size, char *line)
{
	ircserver_t *ircserver = (ircserver_t *)user_data;

    if (!ircserver) return 0;

	switch(status) {

	case LION_CONNECTION_CONNECTED:
		debugf("connected to IRC server\n");
        ircserver->sleep = 30; // connected, set default sleep

        // Send greeting
        if (ircserver->pass)
            if (!lion_printf(handle, "PASS %s\r\n", ircserver->pass)) return -1;
        if (!lion_printf(handle, "NICK %s\r\n", ircserver->nick)) return -1;
        if (!lion_printf(handle, "USER %s FXP.One FXP.Oned :%s\r\n",
                         ircserver->user, ircserver->user)) return -1;
		break;

	case LION_CONNECTION_LOST:
		printf("Failed to connect to IRC server %s:%d: %d\n",
               ircserver->server, ircserver->port, size);

        ircserver->handle = NULL;

        // If we failed to connect (no previous connection) then increase
        // sleep, so we incrementally wait longer to retry
        if (ircserver->sleep < 600) // less thatn 10 minutes?
            ircserver->sleep <<= 1; // .. double sleep
		break;

	case LION_CONNECTION_CLOSED:
		printf("Connection closed to IRC server %s:%d\n",
               ircserver->server, ircserver->port);
        ircserver->handle = NULL;
		break;

	case LION_CONNECTION_SECURE_ENABLED:
		debugf("successfully negotiated SSL on IRC server %s:%d\n",
               ircserver->server, ircserver->port);
		break;

	case LION_CONNECTION_SECURE_FAILED:
		debugf("successfully negotiated SSL on IRC server %s:%d\n",
               ircserver->server, ircserver->port);
        lion_disconnect(handle);
		break;

	case LION_INPUT:
        if (line && *line) {
            //debugf("~%s\n",line);
            irc_server_input(ircserver, line);
        }
	}

	return 0;

}


void irc_add_trade(char *nick, char *match, char *srcsite, char *srcdir,
                   char *dstsite, char *dstdir,
                   char *accept, char *reject, int requeue, int subdir)
{
    trade_t *trade;
	char **dtmp;
    site_t *src, *dst;

    trade = calloc(sizeof(*trade), 1);
    if (!trade) return;

#if HAVE_PCRE && CLOMPS_IRC
    {
        const char *error;
        int erroffset;

        trade->re = pcre_compile(match,
                                 0,
                                 &error,
                                 &erroffset,
                                 NULL);
        if (trade->re == NULL) {
            printf("PCRE: MATCH compilation failed at offset %d: %s\n", erroffset, error);
            SAFE_FREE(trade);
            return ;
        }

        trade->nick_re = pcre_compile(nick,
                                      0,
                                      &error,
                                      &erroffset,
                                      NULL);
        if (trade->nick_re == NULL) {
            printf("PCRE: NICK compilation failed at offset %d: %s\n", erroffset, error);
            SAFE_FREE(trade);
            return ;
        }
    }
#endif

    SAFE_COPY(trade->nick,    nick);
    SAFE_COPY(trade->match,   match);
    SAFE_COPY(trade->srcsite, srcsite);
    SAFE_COPY(trade->srcdir,  srcdir);
    SAFE_COPY(trade->dstsite, dstsite);
    SAFE_COPY(trade->dstdir,  dstdir);
    SAFE_COPY(trade->accept,  accept);
    SAFE_COPY(trade->reject,  reject);
    trade->requeue = requeue;
    trade->subdir = subdir;

    debugf("[irc] added TRADE on MATCH '%s'\n", trade->match);

    // Link-in
    trade->next = irc_trades;
    irc_trades = trade;

    // Add directory
    src = site_find(trade->srcsite);
    if (!src) {
        printf("TRADE SRCSITE=%s but could not find SITE|NAME=%s in conf\n",
               srcsite, srcsite);
        return;
    }
    dst = site_find(trade->dstsite);
    if (!dst) {
        printf("TRADE DSTSITE=%s but could not find SITE|NAME=%s in conf\n",
               dstsite, dstsite);
        return;
    }

    // Add DIR to list:

    dtmp = (char **) realloc(src->dirs,
                             sizeof(char *) * (src->num_dirs + 1));
    if (!dtmp) return;

    src->dirs = dtmp;

    src->dirs[ src->num_dirs ] = strdup(srcdir);
    src->num_dirs++;

    dtmp = (char **) realloc(dst->dirs,
                             sizeof(char *) * (dst->num_dirs + 1));
    if (!dtmp) return;

    dst->dirs = dtmp;

    dst->dirs[ dst->num_dirs ] = strdup(dstdir);
    dst->num_dirs++;


}




