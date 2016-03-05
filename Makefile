# Very simple -*-Makefile-*- for .deb generation

CC       = @gcc
CPPFLAGS = -W -Wall

all: mcjoin

mcjoin: mcjoin.c

package:
	dpkg-buildpackage -B -uc -tc

clean:
	@rm -f mcjoin *.o

distclean: clean

