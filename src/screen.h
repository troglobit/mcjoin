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

#ifndef MCJOIN_SCREEN_H_
#define MCJOIN_SCREEN_H_

#include "config.h"
#include "log.h"

#ifdef HAVE_TERMIOS_H
#include <termios.h>
#endif

/* Esc[2J                     - Clear screen */
#define cls()                 fputs("\e[2J", stderr)
/* Esc[0J                     - Clear screen from cursor down */
#define clsdn()               fputs("\e[0J", stderr)
/* Esc[Line;ColumnH           - Moves the cursor to the specified position (coordinates) */
#define gotoxy(x,y)           fprintf(stderr, "\e[%d;%dH", (int)(y), (int)(x))
/* Esc[?25l (lower case L)    - Hide Cursor */
#define hidecursor()          fputs("\e[?25l", stderr)
/* Esc[?25h (lower case H)    - Show Cursor */
#define showcursor()          fputs("\e[?25h", stderr)

#ifdef HAVE_TERMIOS_H
/*
 * This flag is used on *BSD when calling tcsetattr() to prevent it
 * from changing speed, duplex, parity.  GNU says we should use the
 * CIGNORE flag to c_cflag, but that doesn't exist so ... we rely on
 * our initial tcgetattr() and prey that nothing changes on the TTY
 * before we exit and restore with tcsetattr()
 */
#ifndef TCSASOFT
#define TCSASOFT 0
#endif

int ttraw    (void);
int ttcooked (void);
int ttsize   (int *, int *);

#else
#define ttraw()     0
#define ttcooked()  0
#define ttsize(a,b) 0

#endif /* HAVE_TERMIOS_H */

void progress(void);

#endif /* MCJOIN_SCREEN_H_ */
