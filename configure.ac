AC_PREREQ([2.69])

AC_INIT([gnomeradio],[1.8],[http://bugzilla.gnome.org/enter_bug.cgi?product=gnomeradio],[gnomeradio],[http://projects.gnome.org/gnomeradio/])

AM_MAINTAINER_MODE([enable])
AC_CONFIG_SRCDIR([src/gui.c])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])

AM_INIT_AUTOMAKE([1.11 dist-xz tar-ustar no-dist-gzip check-news])

AM_SILENT_RULES([yes])

GETTEXT_PACKAGE=gnomeradio
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE, ["$GETTEXT_PACKAGE"], [Define to the Gettext package name])
AC_SUBST(GETTEXT_PACKAGE)
IT_PROG_INTLTOOL([0.40.0])

YELP_HELP_INIT

AC_PROG_CXX
AC_PROG_CC
AC_PROG_CC_STDC
AC_PROG_CPP
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
AM_PATH_GLIB_2_0

LT_PREREQ([2.2])
LT_INIT

AC_CHECK_HEADERS([fcntl.h limits.h memory.h stddef.h stdlib.h string.h sys/ioctl.h sys/time.h unistd.h])

AC_TYPE_SSIZE_T
AC_CHECK_MEMBERS([struct stat.st_rdev])

AC_FUNC_LSTAT_FOLLOWS_SLASHED_SYMLINK
AC_HEADER_MAJOR
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_CHECK_FUNCS([atexit memset pow realpath rint strcasecmp strchr strdup strncasecmp strrchr strstr])

AC_CHECK_LIB([pthread], [pthread_create])
PTHREAD_LIBS=-lpthread
AC_SUBST(PTHREAD_LIBS)

AC_CHECK_LIB([m], [pow])
M_LIBS=-lm
AC_SUBST(M_LIBS)

PKG_CHECK_MODULES(ALSA, alsa >= 1.0.25)
AC_SUBST(ALSA_CFLAGS)
AC_SUBST(ALSA_LIBS)

PKG_CHECK_MODULES(GNOME, gtk+-3.0 libxml-2.0 glib-2.0 gio-2.0)
AC_SUBST(GNOME_CFLAGS)
AC_SUBST(GNOME_LIBS)

PKG_CHECK_MODULES(GSTREAMER, gstreamer-1.0 >= 1.0.5 gstreamer-plugins-base-1.0 >= 1.0.5 gstreamer-pbutils-1.0 >= 1.0.5)
AC_SUBST(GSTREAMER_CFLAGS)
AC_SUBST(GSTREAMER_LIBS)

PKG_CHECK_MODULES(XML, libxml-2.0 >= 2.5)
AC_SUBST(XML_CFLAGS)
AC_SUBST(XML_LIBS)

AC_PATH_PROG(GLIB_COMPILE_SCHEMAS, glib-compile-schemas)
GLIB_GSETTINGS

GLIB_COMPILE_RESOURCES=`$PKG_CONFIG gio-2.0 --variable=glib_compile_resources`
AC_SUBST(GLIB_COMPILE_RESOURCES)

AC_SUBST(LIRC)
LIRC=""
if test "$enable_lirc" != "no"; then
  AC_CHECK_LIB(lirc_client, lirc_init,
    AC_DEFINE(HAVE_LIRC, 1, "Is LIRC installed") LIRC="-llirc_client",,)
else
  echo "*** lirc disabled"
fi

AC_CONFIG_FILES([gnomeradio.spec
                 Makefile
                 data/Makefile
                 data/icons/16x16/Makefile
                 data/icons/22x22/Makefile
                 data/icons/24x24/Makefile
                 data/icons/32x32/Makefile
                 data/icons/48x48/Makefile
                 data/icons/Makefile
                 data/icons/scalable/Makefile
                 data/pixmaps/Makefile
                 help/Makefile
                 po/Makefile.in
                 src/Makefile])

AC_OUTPUT

