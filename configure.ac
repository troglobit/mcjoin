AC_INIT(mcjoin, 2.4, https://github.com/troglobit/mcjoin/issues,, https://github.com/troglobit/mcjoin/)
AM_INIT_AUTOMAKE([1.11 foreign no-dist-gzip dist-xz])

AC_CONFIG_SRCDIR([mcjoin.c])
AC_CONFIG_HEADER([config.h])
AC_CONFIG_FILES([Makefile])

AC_PROG_CC
AC_PROG_INSTALL

AC_HEADER_STDC

AC_CHECK_MEMBERS([struct sockaddr_storage.ss_len], , ,
[
#include <sys/socket.h>
])

AC_OUTPUT
