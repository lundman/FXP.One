lib_LIBRARIES=libdirlist.a
libdirlist_a_SOURCES= dirlist.c dirlist.h dirlist_child.c				\
	dirlist_child.h dirlist_child_process.c dirlist_child_process.h		\
	dirlist_child_sort.c dirlist_child_sort.h							\
	dirlist_child_userinput.c dirlist_child_userinput.h					\
	dirlist_userinput.c dirlist_userinput.h queue.c queue.h strmode.c
AM_CPPFLAGS = -I$(srcdir)/../../src

SUBDIRS=win32

# Win32 extras
EXTRA_DIST=dirlist_child_process_win32.c 

