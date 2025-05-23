/*
 * Copyright (c) 2008-2025  Joachim Wiberg <troglobit()gmail!com>
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

#include <fcntl.h>
#include <unistd.h>

int daemonize(void)
{
	int rc = 0;

	/* Going to background ... */
	if ((rc = fork())) {
		if (rc == -1)
			return -1;
		_exit(0);
	}

	/* Start a new session */
	if (setsid() == (pid_t)-1)
		return -1;

	/*
	 * Old double-fork trick to prevent us from reacquiring
	 * any controlling terminal.
	 */
	if ((rc = fork())) {
		if (rc == -1)
			return -1;
		_exit(0);
	}

	/* Neutral directory in case of unmounts */
	rc |= chdir("/");

	/* Going dark */
	rc |= close(0) | close(1) | close(2);

	/* Redirect stdio to /dev/null */
	if (open("/dev/null", O_RDWR) != 0 ||
	    dup(0) == -1 || dup(0) == -1 || rc)
		_exit(1);

	return 0;
}

/**
 * Local Variables:
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
