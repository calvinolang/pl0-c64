/* io_check.c -- the C64 text I/O layer (src/c64/io_c64.{c,s}) in VICE.
 *
 * Run by `make c64-io` (tests/vice_run.sh with --snap, --on key injection
 * and --check tools/check_io.py). All output goes through io_put_str, so
 * this file uses ASCII string literals like the VM's strings (every C64
 * source is compiled with the ASCII charmap, see the Makefile). Each part checks the screen RAM itself; the harness also checks
 * the screens it saved:
 *   step 1: after 60 lines `line 00`..`line 59`: rows 0..23 must be
 *           `line 36`..`line 59`, row 24 empty (the screen scrolled)
 *   step 2: after a 100-character string: rows of 40, 40 and 20 characters
 * Number input: the program prints `feedN` on a line of its own and calls
 * io_read_num; the harness sees the line and types the keys with the VICE
 * monitor `keybuf` command (see Makefile c64-io). io_break: the RUN/STOP
 * code 3 typed into the keyboard buffer, the keyboard matrix forced from
 * the monitor (CIA1 port B bit 7 as an output driven low, which reads as
 * row 7 pressed in every column, RUN/STOP included), and the same paths
 * simulated in memory. Ends with `PASS` or `FAIL n`. */
#include <string.h>
#include <stdint.h>
#include <6502.h>
#include "platform/io.h"
#include "platform/io_c64.h"
#include "platform/num.h"

#define REG(a) (*(volatile uint8_t *)(a))
#define STEP REG(0x02A7)
#define ACK  REG(0x02A8)
#define SCREEN ((volatile uint8_t *)0x0400)
#define JIFFY ((unsigned)REG(0xA1) << 8 | REG(0xA2))   /* jiffy clock, low 16 bits */

static unsigned failures;
static char nb[8];

static void sync(uint8_t n)
{
    STEP = n;
    while (ACK != n) ;
}

static const char *u2s(unsigned v)
{
    char *p = nb + sizeof nb - 1;
    *p = 0;
    do { *--p = (char)('0' + v % 10); v /= 10; } while (v);
    return p;
}

static void say(const char *a, const char *b)
{
    io_put_str(a);
    io_put_str(b);
    io_put_nl();
}

static void check(int ok, const char *what)
{
    if (!ok) { ++failures; say("bad: ", what); }
}

/* screen code of the ASCII characters used in expectations */
static uint8_t code(char c)
{
    if (c >= 'a' && c <= 'z') return (uint8_t)(c - 'a' + 1);
    return (uint8_t)c;              /* digits, space, A-Z */
}

static int row_is(uint8_t r, const char *s)
{
    uint8_t i;
    volatile uint8_t *p = SCREEN + r * 40;
    for (i = 0; i < 40; i++) {
        if (p[i] != (*s ? code(*s++) : 32)) return 0;
    }
    return 1;
}

static char line[8] = "line 00";
static uint8_t scrolled;            /* rows that matched after scrolling */

static void scrolling(void)
{
    uint8_t n, colour = REG(0x0286);
    io_text_open();
    for (n = 0; n < 60; n++) {
        line[5] = (char)('0' + n / 10);
        line[6] = (char)('0' + n % 10);
        REG(0x0286) = (n & 1) ? 7 : 14;     /* odd lines yellow: colour RAM must scroll too */
        io_put_str(line);
        io_put_nl();
    }
    REG(0x0286) = colour;
    for (n = 0; n < 24; n++) {
        line[5] = (char)('0' + (n + 36) / 10);
        line[6] = (char)('0' + (n + 36) % 10);
        if (!row_is(n, line)) break;
        if ((REG(0xD800 + n * 40 + 6) & 15) != (((n + 36) & 1) ? 7 : 14)) break;
    }
    if (n == 24 && row_is(24, "")) n = 25;
    scrolled = n;
    sync(1);
}

static char hundred[101];

static void wrapping(void)
{
    char part[41];
    uint8_t i;
    for (i = 0; i < 100; i++) hundred[i] = (char)('a' + i % 26);
    io_text_open();
    io_put_str(hundred);
    io_put_nl();
    memcpy(part, hundred, 40); part[40] = 0;
    i = row_is(0, part);
    memcpy(part, hundred + 40, 40);
    i = i && row_is(1, part);
    memcpy(part, hundred + 80, 20); part[20] = 0;
    i = i && row_is(2, part) && row_is(3, "");
    sync(2);
    io_text_open();
    check(scrolled == 25, "scroll");
    check(i, "wrap");
}

/* deferred wrap: 40 characters and a new line take one row, and a
 * character after exactly 40 goes to the next row */
static void wrap40(void)
{
    hundred[40] = 0;
    io_text_open();
    io_put_str(hundred);            /* 40 characters */
    io_put_nl();
    io_put_str("x");
    io_put_nl();
    io_put_str(hundred);
    io_put_str("y");                /* no new line: wraps by itself */
    check(row_is(1, "x"), "40 + new line: x on the next row");
    check(row_is(3, "y") && row_is(4, ""), "40 + y: y on the next row");
    check(SCREEN[40 * 2] == 1 && SCREEN[40 * 2 + 39] == 14, "40 characters on row 2");
}

static void charset(void)
{
    /* ASCII only: an 8-bit code such as PETSCII shifted E ($C5) is shown
     * as a space, like the other non-ASCII codes */
    static const uint8_t want[] = {65, 90, 1, 26, 48, 57, 0, 27, 29, 100, 32, 33, 63, 46, 69, 32, 32};
    uint8_t i;
    io_text_open();
    io_put_str("AZaz09@[]_ !?.E\xC5\xDA");
    for (i = 0; i < sizeof want; i++)
        if (SCREEN[i] != want[i]) break;
    check(i == sizeof want, "charset");
    check((REG(0xD018) & 0x0E) == 0x06, "lower/upper charset");
    io_put_nl();
}

/* io_read_num with the keys the harness types after `feedN` */
static void read_one(const char *feed, const char *want)
{
    num_t v;
    char out[16];
    int rc;
    say(feed, "");
    rc = io_read_num(&v);
    if (want) {
        if (rc) { check(0, feed); return; }
        num_format(&v, out);
        if (strcmp(out, want) != 0) { say("got ", out); check(0, feed); return; }
    } else if (!rc) {
        check(0, feed); return;
    }
    say(feed, " ok");
}

/* spin until io_break() or about 60 s of emulated time */
static unsigned wait_break(void)
{
    unsigned t0 = JIFFY, n = 0;
    while (!io_break()) {
        ++n;
        if ((unsigned)(JIFFY - t0) > 3600) return 0;
    }
    return n ? n : 1;
}

/* CIA2 timers as a cycle counter, interrupts off (as in gfx_check.c) */
static void tstart(void)
{
    REG(0xDD0E) = 0; REG(0xDD04) = 0xFF; REG(0xDD05) = 0xFF;
    REG(0xDD0E) = 0x11;
}

static unsigned tstop(void)
{
    REG(0xDD0E) = 0;
    return 0xFFFFu - (REG(0xDD04) | (unsigned)REG(0xDD05) << 8);
}

static void stop_key(void)
{
    unsigned cal, t_idle, t_buf;
    int a, b;
    /* idle: no key */
    check(io_break() == 0 && io_break() == 0, "break idle");
    /* buffer path, simulated: RUN/STOP code 3 queued behind another key */
    SEI();
    REG(0x0277) = 'x'; REG(0x0278) = 3; REG(0x00C6) = 2;
    a = io_break(); b = io_break();
    CLI();
    check(a == 1 && b == 0 && REG(0x00C6) == 0, "break buffer (simulated)");
    /* cost */
    SEI();
    tstart(); cal = tstop();
    tstart(); io_break(); t_idle = tstop() - cal;
    REG(0x0277) = 'x'; REG(0x00C6) = 1;
    tstart(); io_break(); t_buf = tstop() - cal;
    REG(0x00C6) = 0;
    CLI();
    say("break cycles idle ", u2s(t_idle));
    say("break cycles 1 key queued ", u2s(t_buf));
    /* real injection 1: the harness types code 3 (keybuf) */
    say("feed8", "");
    check(wait_break() != 0, "break by keybuf");
    check(io_break() == 0, "break reported once (keybuf)");
    /* real injection 2: the harness forces the matrix (port B bit 7 low) */
    say("feed9", "");
    check(wait_break() != 0, "break by matrix");
    REG(0xDC00) = 0x7F;
    check((REG(0xDC01) & 0x80) == 0, "matrix forced");   /* the harness's press is real */
    a = io_break();                  /* still held: not again */
    REG(0xDC03) = 0;                 /* release: port B back to input */
    REG(0xDC01) = 0xFF;
    b = io_break();                  /* sees the key up: re-arms, 0 */
    check(a == 0 && b == 0, "break reported once (matrix)");
    say("break ok", "");
}

int main(void)
{
    STEP = 0; ACK = 0;
    scrolling();
    wrapping();
    wrap40();
    charset();
    read_one("feed1", "12.5");      /* 12.5 RETURN */
    read_one("feed2", "-3");        /* -3 RETURN */
    read_one("feed3", 0);           /* abc RETURN: bad input */
    read_one("feed4", 0);           /* RETURN alone: bad input */
    read_one("feed5", "7");         /* 1 DEL 7 RETURN: editing */
    read_one("feed6", 0);           /* - RETURN: a sign alone is bad */
    read_one("feed7", 0);           /* 4 RUN/STOP: bad input */
    read_one("feed10", "1.11111111E+19");  /* 25 ones: only 20 kept */
    read_one("feed11", "1000");     /* 1e3 RETURN: lower case e */
    read_one("feed12", "-0.025");   /* -2.5E-2 RETURN: shifted E */
    read_one("feed13", "3");        /* +3 RETURN: plus sign like the host */
    read_one("feed14", "7");        /* space space 7 space RETURN */
    read_one("feed15", 0);          /* -+3 RETURN: second sign */
    stop_key();
    if (failures == 0) io_put_str("PASS");
    else { io_put_str("FAIL "); io_put_str(u2s(failures)); }
    io_put_nl();
    for (;;) ;
    return 0;
}
