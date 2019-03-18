AC_INIT([Network CPU utilisation monitor], [0.1])
AC_CONFIG_SRCDIR([src/netcpu-monitor.c])
AM_INIT_AUTOMAKE([subdir-objects])
AC_PROG_CC
LIBS="$LIBS -lpthread -lncurses"
CFLAGS="$CFLAGS -Wall"
AC_CONFIG_FILES([Makefile src/Makefile])
AC_OUTPUT
