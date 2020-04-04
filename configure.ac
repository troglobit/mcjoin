AC_INIT(mcjoin, 2.6-beta1, https://github.com/troglobit/mcjoin/issues,, https://github.com/troglobit/mcjoin/)
AC_CONFIG_AUX_DIR(aux)
AM_INIT_AUTOMAKE([1.11 foreign no-dist-gzip dist-xz])

AC_CONFIG_SRCDIR([src/mcjoin.c])
AC_CONFIG_HEADER([config.h])
AC_CONFIG_FILES([Makefile src/Makefile])

AC_PROG_CC
AC_PROG_INSTALL

AC_HEADER_STDC

AC_CHECK_MEMBERS([struct sockaddr_storage.ss_len], , ,
[
#include <sys/socket.h>
])

AC_OUTPUT
