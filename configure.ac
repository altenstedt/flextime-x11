#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT(flextime, 0.14, martin.altenstedt@gmail.com)
AM_INIT_AUTOMAKE([-Wall -Werror foreign])
AC_CONFIG_SRCDIR([main.c])
AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_PROG_CC

# Checks for libraries.
AC_SEARCH_LIBS([XOpenDisplay], [X11])
AC_SEARCH_LIBS([XFree], [X11])
AC_SEARCH_LIBS([XCloseDisplay], [X11])
AC_SEARCH_LIBS([XScreenSaverQueryInfo], [Xss])
AC_SEARCH_LIBS([XScreenSaverAllocInfo], [Xss])

# Checks for header files.
AC_PATH_X
AC_CHECK_HEADERS([X11/extensions/scrnsaver.h])

# Checks for typedefs, structures, and compiler characteristics.

# Checks for library functions.

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
