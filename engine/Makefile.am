bin_PROGRAMS=FXP.Oned
FXP_Oned_SOURCES=engine.c settings.c debug.c parser.c users.c			\
                 command.c command2.c sites.c session.c data.c			\
                 manager.c file.c handler.c queue.c local.c command.h	\
                 file.h queue.h users.h command2.h handler.h session.h	\
                 version.h data.h local.h session_states.h debug.h		\
                 manager.h settings.h engine.h parser.h sites.h http.c http.h

FXP_Oned_CFLAGS=-I$(top_srcdir)/lion/src -I$(top_srcdir)/lion/contrib/libdirlist
FXP_Oned_LDADD=$(top_srcdir)/lion/src/liblion.a $(top_srcdir)/lion/contrib/libdirlist/libdirlist.a
FXP_Oned_CPPFLAGS=-DWFXPROOTDIR='"$(datarootdir)/wfxp"'

# Extra all
EXTRA_DIST=TODO.txt

# Extra Win32
EXTRA_DIST+=getopt.c

# Automatically increase build version if found.
all-local:
	@if test -n "$(BUILD)"; then "$(BUILD)" ; fi
