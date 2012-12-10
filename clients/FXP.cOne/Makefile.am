if WANT_FXP_CONE
  bin_PROGRAMS=FXP.cOne
endif

FXP_cOne_SOURCES= main.c traverse.c connect.c engine.c parser.c			\
				  sitemgr.c display.c file.c qmgr.c qlist.c site.c		\
				  connect.h engine.h main.h qlist.h site.h traverse.h	\
				  display.h file.h parser.h qmgr.h sitemgr.h version.h

FXP_cOne_CFLAGS=-I$(top_srcdir)/lion/src
FXP_cOne_LDADD=$(top_srcdir)/lion/src/liblion.a -l$(curseslib) -lcdk


# Extra all
EXTRA_DIST=TODO.txt


# Automatically increase build version if found.
all-local:
	@if test -n "$(BUILD)"; then "$(BUILD)" ; fi
