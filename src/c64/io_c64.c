/* C64 implementation of the text I/O layer (src/platform/io.h, io_c64.h).
 *
 * The output side (io_text_open, io_put_str, io_put_nl) and io_break are in
 * io_c64.s, which describes the screen, the cursor, scrolling and the
 * character mapping; the same code in cc65 C was about 300 bytes larger.
 * This file reads numbers.
 *
 * io_read_num: prints `? ` at the cursor and reads a line of at most 20
 * characters: keys from the KERNAL buffer through cc65 cgetc (GETIN), each
 * echoed at the cursor, which shows as a reverse-video space. RETURN ends,
 * DEL erases, RUN/STOP (code 3) gives up: bad input (and the press is
 * used up for io_break). Keys arrive in PETSCII: unshifted letters $41-$5A
 * are lower case, shifted $C1-$DA upper case; both are stored as ASCII
 * (this is the keyboard boundary: PETSCII in, ASCII out; num_parse takes
 * 'e' and 'E' like the host). Leading and trailing spaces are
 * skipped (as the host io_read_num skips white space). A leading - is
 * handled here and the rest parsed and negated; a second sign after it
 * (--3, -+3) is bad input, while a plain +3 is passed to num_parse, which
 * takes it like the host's strtod. Empty or malformed input returns 1.
 * Like every C64 source this file is compiled with the ASCII charmap
 * (Makefile); the PETSCII key codes are written as numbers.
 * Not reentrant. */
#include <conio.h>
#include "core/ascii.h"
#include "platform/io.h"
#include "platform/io_c64.h"

#define MAXIN 20                /* io_read_num line length */

/* io_c64.s */
extern uint8_t io_brk_armed;
void __fastcall__ io_putc(uint8_t c);   /* ASCII at the cursor, advance */
void __fastcall__ io_show(uint8_t sc);  /* screen code at the cursor, stay */
void io_back(void);                     /* cursor one place back */

/* File statics instead of locals: smaller cc65 code. */
static uint8_t n, c;
static char in[MAXIN + 1];

int io_read_num(num_t *r)
{
    const char *p = in;
    io_put_str("? ");
    n = 0;
    for (;;) {
        io_show(0xA0);                              /* cursor: reverse space */
        c = cgetc();
        if (c == 0x0D || c == 0x03) break;          /* RETURN, RUN/STOP */
        if (c == 0x14) {                            /* DEL */
            if (n) {
                io_show(32);
                io_back();
                --n;
            }
            continue;
        }
        if (c >= 0xC1 && c <= 0xDA) c -= 0x80;      /* shifted: A-Z */
        else if (c >= 0x41 && c <= 0x5A) c += 0x20; /* unshifted: a-z */
        else if (c < 0x20 || c > 0x3F) continue;    /* no other keys */
        if (n < MAXIN) {
            io_putc(c);
            in[n++] = c;
        }
    }
    io_show(32);
    io_put_nl();
    if (c == 0x03) {
        io_brk_armed = 0;                           /* this press is used up */
        return 1;
    }
    while (n && in[n - 1] == 0x20) --n;             /* trailing spaces */
    in[n] = 0;
    while (*p == 0x20) ++p;                         /* leading spaces */
    c = 0;
    if (*p == 0x2D) {                               /* '-': negate below */
        ++p;
        c = 1;
        if (*p == 0x2B || *p == 0x2D) return 1;     /* no second sign */
    }
    if (num_parse(p, r)) return 1;                  /* takes a leading '+' */
    if (c) num_neg(r, r);
    return 0;
}
