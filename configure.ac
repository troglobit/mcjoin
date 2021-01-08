AC_INIT(mcjoin, 2.8, https://github.com/troglobit/mcjoin/issues,, https://github.com/troglobit/mcjoin/)
AC_CONFIG_AUX_DIR(aux)
AM_INIT_AUTOMAKE([1.11 foreign])

AC_CONFIG_SRCDIR([src/mcjoin.c])
AC_CONFIG_HEADER([config.h])
AC_CONFIG_FILES([Makefile src/Makefile])

AC_PROG_CC
AC_PROG_INSTALL

AC_HEADER_STDC

AC_CHECK_HEADERS([termios.h utility.h])
AC_CHECK_MEMBERS([struct sockaddr_storage.ss_len], , ,
[
#include <sys/socket.h>
])
AC_CHECK_MEMBERS([struct ip_mreqn.imr_ifindex], , ,
[
#include <netinet/in.h>
])

# Check for usually missing API's
AC_REPLACE_FUNCS([strlcpy])
AC_CONFIG_LIBOBJ_DIR([lib])

# Check build host, differnt for each operating system
AC_CANONICAL_HOST

case $host_os in
    dragonfly*)
	CPPFLAGS="-I../include/dragonfly"
	;;
    netbsd*)
        CPPFLAGS="$CPPFLAGS -D_OPENBSD_SOURCE"
        ;;
    solaris*)
	CPPFLAGS="-DSYSV -D_XPG4_2 -D__EXTENSIONS__"
	LIBS="-lsocket -lnsl"
	;;
    linux*)
	# -D_GNU_SOURCE Use GNU extensions, where possible (GLIBC)
	# -D_BSD_SOURCE Use functions derived from 4.3 BSD Unix rather than POSIX.1
	#               In GLIBC >= v2.20 this is replaced with -D_DEFAULT_SOURCE,
	#               but to build on older GLIBC systems we now need both ...
	CFLAGS="-O2 -g -std=gnu99"
	CPPFLAGS="-D_GNU_SOURCE -D_FORTIFY_SOURCE=2"
	;;
    *)
        ;;
esac

AC_OUTPUT
