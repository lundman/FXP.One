
AC_INIT([FXP.One],[2.1], [bug-report@lundman.net])
AC_CONFIG_AUX_DIR([build-aux])
AM_INIT_AUTOMAKE([foreign])

# The big search for OpenSSL
# copied from openssh's configure.ac, hopefully correct
tryssldir=""
AC_ARG_WITH(ssl-dir,
        [  --with-ssl-dir=PATH     Specify path to OpenSSL installation ],
        [
                if test "x$withval" != "xno" ; then
                        tryssldir=$withval
                fi
        ]
)
trycdkdir=""
AC_ARG_WITH(cdk-dir,
        [  --with-cdk-dir=PATH     Specify path to CDK installation ],
        [
                if test "x$withval" != "xno" ; then
                        trycdkdir=$withval
                fi
        ]
)

trycursesdir=""
AC_ARG_WITH(curses-dir,
        [  --with-curses-dir=PATH     Specify path to [n]curses installation ],
        [
                if test "x$withval" != "xno" ; then
                        trycursesdir=$withval
                fi
        ]
)

AC_CHECK_PROG(BUILD, build, build)
#AC_CONDITIONAL(BUILD)

# Run ./configure in lion
AC_CONFIG_SUBDIRS([lion])

AC_CHECK_FILE([lion/src/lion.h], ,
   AC_MSG_ERROR([lion.h not found in lion/src/ directory]))

AC_PROG_CC
AC_PROG_RANLIB

AC_TYPE_SIGNAL

if test "x$ac_cv_prog_gcc" = xyes ; then
   CFLAGS="$CFLAGS -Wall -g"
fi

AC_HEADER_TIME
AC_CHECK_HEADERS([stdarg.h varargs.h sys/time.h sys/types.h stat.h sys/stat.h dirent.h limits.h sys/syslimits.h])

AC_CHECK_FUNC([snprintf],
		[AC_DEFINE(HAVE_SNPRINTF,1,[Presence of snprintf found])],
		[AC_MSG_WARN(Adding internal snprintf function)])

AC_PREFIX_DEFAULT(/usr/local/)

AC_SEARCH_LIBS([gethostbyname], [nsl])
AC_SEARCH_LIBS([socket], [socket inet])

# Solaris' ncurses includes stdbool.h that just don't compile unless
# _STDC_C99 is set. Which should check for this better.
#CPPFLAGS="-D_STDC_C99 $CPPFLAGS"

AC_SYS_LARGEFILE

dnl look for curses lib to use
dnl
AC_CACHE_CHECK([for curses library], ac_cv_curses_dir, [
  saved_LIBS="$LIBS"
  saved_LDFLAGS="$LDFLAGS"
  saved_CPPFLAGS="$CPPFLAGS"
  curses_found=no

  for curseslib in ncurses  ; do

  for dir in $trycursesdir "" $prefix /usr/local /opt /usr/pkg /usr ; do
    LDFLAGS="$saved_LDFLAGS"

    # Skip the directory if it isn't there.
    if test ! -z "$dir" -a ! -d "$dir" ; then
       continue;
    fi
    if test ! -z "$dir" ; then
      if test -d "$dir/lib" ; then
        LDFLAGS="-L$dir/lib $LDFLAGS"
      else
        LDFLAGS="-L$dir $LDFLAGS"
      fi
      if test -d "$dir/include/$curseslib/" ; then
        CPPFLAGS="-I$dir/include/$curseslib/ $CPPFLAGS"
      else
        CPPFLAGS="-I$dir $CPPFLAGS"
      fi
      if test -d "$dir/include" ; then
        CPPFLAGS="-I$dir/include $CPPFLAGS"
      else
        CPPFLAGS="-I$dir $CPPFLAGS"
      fi
    fi


	AC_MSG_NOTICE([Trying to link with -l$curseslib ...])

    LIBS="$saved_LIBS -l$curseslib"

    # Can I link it?
    AC_TRY_LINK([#include <$curseslib.h>],
       [ setupterm((void*)0, 0, (void *)0); ],
       [ curses_linked=yes ], [ curses_linked=no ])
    if test $curses_linked = yes; then
       if test ! -z "$dir" ; then
         ac_cv_curses_dir=$dir
       else
         ac_cv_curses_dir="(system)"
       fi
       curses_found=yes
       break
    fi
  done  # for ncurses curses pdcurses
  if test $curses_found = yes ; then
    break;
  fi
  done  # for each path

  LIBS="$saved_LIBS"
  LDFLAGS="$saved_LDFLAGS"
  CPPFLAGS="$saved_CPPFLAGS"
  if test $curses_found = no ; then
  	AC_MSG_WARN([no])
    AC_MSG_WARN([Could not find ncurses, curses or pdcurses. You can specify an explicit path using --with-curses-dir])
  fi
])

if test $curses_found = yes ; then
  AC_DEFINE([HAVE_LIBCURSES], 1, [Define if we found curses])
  #Do not include ncurses for all projects.
  #LIBS="$LIBS -l$curseslib"
  AC_SUBST([curseslib])
  if test "$ac_cv_curses_dir" != "(system)"; then
    if test -d "$ac_cv_curses_dir/lib" ; then
      LDFLAGS="-L$ac_cv_curses_dir/lib $LDFLAGS"
      curses_libdir="$ac_cv_curses_dir/lib"
    else
      LDFLAGS="-L$ac_cv_curses_dir $LDFLAGS"
      curses_libdir="$ac_cv_curses_dir"
    fi
    if test -d "$ac_cv_curses_dir/include/ncurses" ; then
      CPPFLAGS="-I$ac_cv_curses_dir/include/ncurses $CPPFLAGS"
    else
      CPPFLAGS="-I$ac_cv_curses_dir $CPPFLAGS"
    fi
    if test -d "$ac_cv_curses_dir/include" ; then
      CPPFLAGS="-I$ac_cv_curses_dir/include $CPPFLAGS"
    else
      CPPFLAGS="-I$ac_cv_curses_dir $CPPFLAGS"
    fi
  fi
  AC_MSG_NOTICE([Using library $lib...])
fi


dnl look for RIGHT CDK traverseCDKScreen
dnl
AC_CACHE_CHECK([for CDK library], ac_cv_cdk_dir, [
  saved_LIBS="$LIBS"
  saved_LDFLAGS="$LDFLAGS"
  saved_CPPFLAGS="$CPPFLAGS"
  cdk_found=no

  for dir in $trycdkdir "" $prefix /usr/local/ /opt/ /usr/pkg/ /usr/ ; do
    LDFLAGS="$saved_LDFLAGS"
    LIBS="$saved_LIBS -l$curseslib -lcdk"

    # Skip the directory if it isn't there.
    if test ! -z "$dir" -a ! -d "$dir" ; then
       continue;
    fi
    if test ! -z "$dir" ; then
      if test -d "$dir/lib" ; then
        LDFLAGS="-L$dir/lib $LDFLAGS"
      else
        LDFLAGS="-L$dir $LDFLAGS"
      fi
      if test -d "$dir/include/cdk/" ; then
        CPPFLAGS="-I$dir/include/cdk/ $CPPFLAGS"
      else
        CPPFLAGS="-I$dir $CPPFLAGS"
      fi
      if test -d "$dir/include" ; then
        CPPFLAGS="-I$dir/include $CPPFLAGS"
      else
        CPPFLAGS="-I$dir $CPPFLAGS"
      fi
    fi
    # Can I link it?
    AC_TRY_LINK([#include <cdk/cdk.h>],
       [ traverseCDKScreen((void*)0); ],
       [ cdk_linked=yes ], [ cdk_linked=no ])
    if test $cdk_linked = yes; then
       if test ! -z "$dir" ; then
         ac_cv_cdk_dir=$dir
       else
         ac_cv_cdk_dir="(system)"
       fi
       cdk_found=yes
       break
    fi
  done
  LIBS="$saved_LIBS"
  LDFLAGS="$saved_LDFLAGS"
  CPPFLAGS="$saved_CPPFLAGS"
  if test $cdk_found = no ; then
  	AC_MSG_WARN([no])
    AC_MSG_WARN([Could not find a linkable cdk with traverseCDKScreen. Make sure you have installed CDK from http://invisible-island.net/cdk/. You can specify an explicit path using --with-cdk-dir])
  fi
])

if test $cdk_found = yes ; then
  AC_DEFINE([HAVE_LIBCDK], 1, [Define if we found CDK])
# Do not link against cdk for all projects.
#  LIBS="$LIBS -lcdk"
  if test "$ac_cv_cdk_dir" != "(system)"; then
    if test -d "$ac_cv_cdk_dir/lib" ; then
      LDFLAGS="-L$ac_cv_cdk_dir/lib $LDFLAGS"
      cdk_libdir="$ac_cv_cdk_dir/lib"
    else
      LDFLAGS="-L$ac_cv_cdk_dir $LDFLAGS"
      cdk_libdir="$ac_cv_cdk_dir"
    fi
    if test -d "$ac_cv_cdk_dir/include/cdk" ; then
      CPPFLAGS="-I$ac_cv_cdk_dir/include/cdk $CPPFLAGS"
    else
      CPPFLAGS="-I$ac_cv_cdk_dir $CPPFLAGS"
    fi
    if test -d "$ac_cv_cdk_dir/include" ; then
      CPPFLAGS="-I$ac_cv_cdk_dir/include $CPPFLAGS"
    else
      CPPFLAGS="-I$ac_cv_cdk_dir $CPPFLAGS"
    fi
  fi
fi


AM_CONDITIONAL([WANT_FXP_CONE], [test "$cdk_found" = yes])



dnl ------------------------------------------------------
dnl Where do you live, openssl?  And how do we call you?

AC_CACHE_CHECK([for OpenSSL directory], ac_cv_openssl_dir, [
  saved_LIBS="$LIBS"
  saved_LDFLAGS="$LDFLAGS"
  saved_CPPFLAGS="$CPPFLAGS"
  ssl_found=no
  for ssldir in $tryssldir "" $prefix /usr/local/openssl /usr/lib/openssl /usr/local/ssl /usr/lib/ssl /usr/local /usr/athena /usr/pkg /opt /opt/openssl ; do
    LDFLAGS="$saved_LDFLAGS"
    LIBS="$saved_LIBS -lssl -lcrypto"

    # Skip the directory if it isn't there.
    if test ! -z "$ssldir" -a ! -d "$ssldir" ; then
       continue;
    fi
    if test ! -z "$ssldir" ; then
      if test -d "$ssldir/lib" ; then
        LDFLAGS="-L$ssldir/lib $LDFLAGS"
      else
        LDFLAGS="-L$ssldir $LDFLAGS"
      fi
      if test -d "$ssldir/include" ; then
        CPPFLAGS="-I$ssldir/include $CPPFLAGS"
      else
        CPPFLAGS="-I$ssldir $CPPFLAGS"
      fi
    fi
    # Can I link it?
    AC_TRY_LINK([#include <openssl/rand.h>],
       [ RAND_add((void*)0,0,0); ],
       [ openssl_linked=yes ], [ openssl_linked=no ])
    if test $openssl_linked = yes; then
       if test ! -z "$ssldir" ; then
         ac_cv_openssl_dir=$ssldir
       else
         ac_cv_openssl_dir="(system)"
       fi
       ssl_found=yes
       break
    fi
  done
  LIBS="$saved_LIBS"
  LDFLAGS="$saved_LDFLAGS"
  CPPFLAGS="$saved_CPPFLAGS"
  if test $ssl_found = no ; then
  	AC_MSG_WARN([no])
    AC_MSG_WARN([Could not find a linkable OpenSSL. You can specify an explicit path using --with-ssl-dir])
  fi
])

if test $ssl_found = yes ; then
  AC_DEFINE([WITH_SSL], 1, [Define if we found OpenSSL - Legacy name])
  LIBS="$LIBS -lssl -lcrypto"
  if test "$ac_cv_openssl_dir" != "(system)"; then
    if test -d "$ac_cv_openssl_dir/lib" ; then
      LDFLAGS="-L$ac_cv_openssl_dir/lib $LDFLAGS"
      ssl_libdir="$ac_cv_openssl_dir/lib"
    else
      LDFLAGS="-L$ac_cv_openssl_dir $LDFLAGS"
      ssl_libdir="$ac_cv_openssl_dir"
    fi
    if test -d "$ac_cv_openssl_dir/include" ; then
      CPPFLAGS="-I$ac_cv_openssl_dir/include $CPPFLAGS"
    else
      CPPFLAGS="-I$ac_cv_openssl_dir $CPPFLAGS"
    fi
  fi
fi

if test -z $CROSS_COMPILE
then
AC_CACHE_CHECK([whether we need extra options to link OpenSSL],
               ac_cv_openssl_linker_option, [
    saved_LDFLAGS="$LDFLAGS"
    ssl_runs=no
    linked_with=nothing
    for ssl_extra in "" "-Wl,-R$ssl_libdir" "-R$ssl_libdir" ; do
      LDFLAGS="$ssl_extra $saved_LDFLAGS"
      AC_TRY_RUN([
#include <string.h>
#include <openssl/rand.h>
int main(void)
{
        char a[2048];
        memset(a, 0, sizeof(a));
        RAND_add(a, sizeof(a), sizeof(a));
        return(RAND_status() <= 0);
}
                        ],
                openssl_runs=yes, openssl_runs=no)
      if test $openssl_runs = yes ; then
        if test "$linked_with" = nothing; then
           linked_with="$ssl_extra"
        fi
        AC_TRY_RUN([
#include <openssl/opensslv.h>
#include <openssl/crypto.h>
int main(void) {
return (OPENSSL_VERSION_NUMBER == SSLeay()) == 0;
}],
        right_version=yes, right_version=no)
        if test "$right_version" = yes; then
          if test -z "$ssl_extra" ; then
            ac_cv_openssl_linker_option='(none)'
          else
            ac_cv_openssl_linker_option=$ssl_extra
          fi
          ssl_runs=yes
          break
        fi
     fi
    done
    if test $ssl_runs = no ; then
      if test "$linked_with" = 'nothing' ; then
        AC_MSG_ERROR([Found linkable OpenSSL in $ac_cv_openssl_dir, but it doesn't run, even with -R.  Maybe specify another using --with-ssl-dir?])
      else
        if test -z "$linked_with" ; then
            ac_cv_openssl_linker_option='(none)'
        else
            ac_cv_openssl_linker_option=$linked_with
        fi
        AC_MSG_WARN([I managed to make OpenSSL link and run, but I couldn't make it link against the same version I found header files for.])
      fi
    fi
    LDFLAGS="$saved_LDFLAGS"
])

if test "$ac_cv_openssl_linker_option" != '(none)' ; then
   LDFLAGS="$ac_cv_openssl_linker_option $LDFLAGS"
fi
fi

m4_pattern_forbid([^_?PKG_[A-Z_]+$],[pkg.m4 missing, please install pkg-config])
PKG_PROG_PKG_CONFIG
if test -n $PKG_CONFIG; then
   PKG_CHECK_MODULES([PCRE], [libpcre],
   AC_DEFINE([HAVE_PCRE], [1], [Could find PCRE library]),
   AC_MSG_WARN([No PCRE (libpcre - perl regexp library) clomps-irc client will be disabled]))
else
   AC_CHECK_PROG(PCRE_CONFIG, pcre-config, pcre-config)
   AC_ARG_WITH(pcre-config,
   AS_HELP_STRING([--with-pcre-config=PATH],
                   [Location of PCRE pcre-config (auto)]),
	               [pcre_config="$withval"],
	               [pcre_config=""])

  if test "x$pcre_config" != "x" ; then
    AC_MSG_CHECKING(for $pcre_config)

    if test -f $pcre_config ; then
      PCRE_CONFIG=$pcre_config
      AC_MSG_RESULT(yes)
    else
      AC_MSG_RESULT(no - searching PATH)
    fi
  fi
  if test "x$PCRE_CONFIG" = "x"; then
    AC_CHECK_PROGS(PCRE_CONFIG, pcre-config)
  fi
  PCRE_CFLAGS=`$PCRE_CONFIG --cflags`
  PCRE_LIBS=`$PCRE_CONFIG --libs`
  AC_DEFINE([HAVE_PCRE], [1], [Could find PCRE library])
fi
AC_SUBST(PCRE_CFLAGS)
AC_SUBST(PCRE_LIBS)

AM_CONFIG_HEADER([config.h])
AC_CONFIG_FILES([Makefile engine/Makefile clients/Makefile clients/FXP.cOne/Makefile clients/clomps/Makefile clients/wfxp/Makefile])
AC_OUTPUT

