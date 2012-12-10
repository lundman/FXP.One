#ifndef IRC_H_INCLUDED
#define IRC_H_INCLUDED

#if HAVE_PCRE
#include <pcre.h>
#endif

#include "autoq.h"

struct ircchannel_struct {
    char *channel;

    struct ircchannel_struct *next;
};

typedef struct ircchannel_struct ircchannel_t;

struct ircserver_struct {
    lion_t *handle;

    char *server;
    int   port;
    char *pass;
    char *nick;
    char *user;
    int   ssl;

    ircchannel_t *channels;

    time_t last_time;
    time_t sleep;

    struct ircserver_struct *next;
};

typedef struct ircserver_struct ircserver_t;

struct trade_struct {
    char *nick;
    char *match;
    char *srcsite;
    char *srcdir;
    char *dstsite;
    char *dstdir;
    char *accept;
    char *reject;
#if HAVE_PCRE
    pcre *re;
    pcre *nick_re;
#endif
    autoq_t *aq;

    int requeue;
    int subdir;

    struct trade_struct *next;
};

typedef struct trade_struct trade_t;



ircserver_t *irc_addserver ( char *server, char *port, char *pass, char *nick,
                             char *user, char *ssl );
void         irc_addchannel( ircserver_t *ircserver, char *channel );

int          irc_init ( void );
void         irc_free ( void );
int          irc_server_handler(lion_t *handle, void *user_data,
                                int status, int size, char *line);
void         irc_add_trade ( char *nick, char *match, char *srcsite, char *srcdir,
                             char *dstsite, char *dstdir,
                             char *accept, char *reject, int requeue, int subdir);
void         clomps_ircqueue( trade_t *trade, char *releasename,
                              ircserver_t *ircserver, char *channel,
                              char *from );

#endif
