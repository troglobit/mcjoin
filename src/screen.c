/*
 * Copyright (c) 2020  Joachim Wiberg <troglobit@gmail.com>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

#include "screen.h"

#include <poll.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

#ifdef HAVE_TERMIOS_H
static struct	termios	oldtty;			/* POSIX tty settings. */
static struct	termios	newtty;

/*
 * This function sets the terminal to RAW mode, as defined for the current
 * shell.  This is called both by ttopen() above and by spawncli() to
 * get the current terminal settings and then change them to what
 * mg expects.	Thus, tty changes done while spawncli() is in effect
 * will be reflected in mg.
 */
int ttraw(void)
{
	if (tcgetattr(0, &oldtty) == -1) {
		printf("can't get terminal attributes\n");
		return 1;
	}
	(void)memcpy(&newtty, &oldtty, sizeof(newtty));
	/* Set terminal to 'raw' mode and ignore a 'break' */
	newtty.c_cc[VMIN] = 1;
	newtty.c_cc[VTIME] = 0;
	newtty.c_iflag |= IGNBRK;
	newtty.c_iflag &= ~(BRKINT | PARMRK | INLCR | IGNCR | ICRNL | IXON);
	newtty.c_oflag &= ~OPOST;
	newtty.c_lflag &= ~(ECHO | ECHONL | ICANON | IEXTEN);//ISIG

	if (tcsetattr(0, TCSASOFT | TCSADRAIN, &newtty) == -1) {
		printf("can't tcsetattr\n");
		return 1;
	}

	return 0;
}

/*
 * This function restores all terminal settings to their default values,
 * in anticipation of exiting or suspending the editor.
 */
int ttcooked(void)
{
	if (tcsetattr(0, TCSASOFT | TCSADRAIN, &oldtty) == -1) {
		printf("can't tcsetattr\n");
		return 1;
	}
	return 0;
}

int ttwidth(void)
{
	struct pollfd fd = { STDIN_FILENO, POLLIN, 0 };
	struct termios tc, saved;
	char buf[42];
	int rc = 79;

	memset(buf, 0, sizeof(buf));
	tcgetattr(STDERR_FILENO, &tc);
	saved = tc;
	tc.c_cflag |= (CLOCAL | CREAD);
	tc.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
	tcsetattr(STDERR_FILENO, TCSANOW, &tc);
	fprintf(stderr, "\e7\e[r\e[999;999H\e[6n");

	if (poll(&fd, 1, 300) > 0) {
		int row, col;

		if (scanf("\e[%d;%dR", &row, &col) == 2)
			rc = col;
	}

	fprintf(stderr, "\e8");
	tcsetattr(STDERR_FILENO, TCSANOW, &saved);

	return rc;
}
#endif /* HAVE_TERMIOS_H */

void progress(void)
{
	const char *style = ".oOOo.";
	static unsigned int i = 0;
	size_t num = 6;

	if (logon())
		return;

	if (!(i % num))
		printf(".");

	putchar(style[i++ % num]);
	putchar('\b');

	fflush(stdout);
}
