# Very simple -*-Makefile-*- for .deb generation
CFLAGS = -W -Wall -Wextra

all: mcjoin

mcjoin: mcjoin.c

package:
	dpkg-buildpackage -B -uc -tc

clean:
	@rm -f mcjoin *.o

distclean: clean

