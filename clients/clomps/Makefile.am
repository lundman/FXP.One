bin_PROGRAMS=clomps clomps-irc

clomps_SOURCES= clomps.c irc.c irc.h debug.c parser.c site.c conf.c file.c	\
                autoq.c autoq_cmd.c autoq.h clomps.h debug.h parser.h	\
                version.h autoq_cmd.h conf.h file.h site.h
clomps_CFLAGS=-I$(top_srcdir)/lion/src
clomps_LDADD=$(top_srcdir)/lion/src/liblion.a

clomps_irc_SOURCES= clomps-irc.c irc.c irc.h debug.c parser.c site.c conf.c	\
                file.c autoq.c autoq_cmd.c autoq.h clomps.h debug.h		\
                parser.h version.h autoq_cmd.h conf.h file.h site.h
clomps_irc_CFLAGS=-I$(top_srcdir)/lion/src -DCLOMPS_IRC=1
clomps_irc_LDADD=$(top_srcdir)/lion/src/liblion.a @PCRE_LIBS@

INCLUDES=@PCRE_CFLAGS@

# Extra all
EXTRA_DIST=README.txt getopt.c clomps.conf irc.conf


