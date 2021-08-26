#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT(flextime, 0.18.0, martin.altenstedt@gmail.com)
AM_INIT_AUTOMAKE([-Wall -Werror])
AC_CONFIG_SRCDIR([daemon.c])
AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_PROG_CC
AC_PATH_PROG(PROTOC, protoc-c)

# Checks for libraries.
AC_SEARCH_LIBS([XOpenDisplay], [X11])
AC_SEARCH_LIBS([XFree], [X11])
AC_SEARCH_LIBS([XCloseDisplay], [X11])
AC_SEARCH_LIBS([XScreenSaverQueryInfo], [Xss])
AC_SEARCH_LIBS([XScreenSaverAllocInfo], [Xss])
AC_SEARCH_LIBS([protobuf_c_message_pack], [protobuf-c])
AC_SEARCH_LIBS([argp_parse], [argp])

# Checks for header files.
AC_PATH_X
AC_CHECK_HEADERS([X11/extensions/scrnsaver.h])

# Checks for typedefs, structures, and compiler characteristics.

# Checks for library functions.

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
