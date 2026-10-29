AUTOMAKE_OPTIONS  = subdir-objects
bin_PROGRAMS      = mcjoin
mcjoin_SOURCES	  = mcjoin.c mcjoin.h		\
		    addr.c addr.h		\
		    inetaddr.c inetaddr.h	\
		    daemonize.c			\
		    log.c log.h			\
		    pev.c pev.h			\
		    queue.h			\
		    receiver.c sender.c		\
		    screen.c screen.h
mcjoin_LDADD      = $(LIBS) $(LIBOBJS)
mcjoin_CFLAGS     = -W -Wall -Wextra

check_PROGRAMS         = pev-timer-test
TESTS                  = $(check_PROGRAMS)
pev_timer_test_SOURCES = pev_timer_test.c pev.c pev.h
pev_timer_test_CFLAGS  = -W -Wall -Wextra
