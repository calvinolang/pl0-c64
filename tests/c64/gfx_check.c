/* gfx_check.c -- the C64 bitmap layer (src/c64/gfx_c64.{c,s}) in VICE.
 *
 * Run by `make c64-gfx`: tests/vice_run.sh --snap ... --check
 * tools/check_bitmap.py. The program and the harness meet at three steps
 * (handshake bytes STEP = $02A7, ACK = $02A8, free KERNAL RAM): at each step
 * the program writes the step number to STEP and waits until the harness,
 * having saved all 64 KB of RAM to snapN.bin, writes the same number to ACK.
 *   step 1: before gfx_open (text screen, nothing drawn)
 *   step 2: after gfx_open (bitmap cleared, matrix set, nothing drawn)
 *   step 3: after drawing gfx_lines.h (plus the timed lines, which repeat
 *           pixels already drawn); $02A9-$02AD hold $DD00, $D011, $D016,
 *           $D018 and $D020 as read while the bitmap is shown
 * tools/check_bitmap.py compares the snapshots (no stray writes) and the
 * bitmap with the host renderer. After step 3 the program closes the
 * bitmap and checks by itself that the VIC registers and the text screen
 * are back as they were, prints the timings and `PASS` or `FAIL n`. */
#include <string.h>
#include <stdint.h>
#include <6502.h>
#include "platform/gfx.h"
#include "platform/io_c64.h"
#include "gfx_lines.h"
#include "out.h"

#define REG(a) (*(volatile uint8_t *)(a))
#define STEP REG(0x02A7)
#define ACK  REG(0x02A8)

static unsigned failures;
static uint8_t screen[1000], colours[1000];
static uint8_t r_dd00, r_d011, r_d016, r_d018, r_d020, r_d021;
static unsigned long t_cal, t_diag, t_horiz, t_steep, t_far, t_dot;

static void sync(uint8_t n)
{
    STEP = n;
    while (ACK != n) ;
}

/* CIA2 timers as a 32-bit cycle counter: timer A counts phi2 from $FFFF,
 * timer B counts timer A underflows. Called with interrupts off. */
static void tstart(void)
{
    REG(0xDD0E) = 0; REG(0xDD0F) = 0;
    REG(0xDD04) = 0xFF; REG(0xDD05) = 0xFF;
    REG(0xDD06) = 0xFF; REG(0xDD07) = 0xFF;
    REG(0xDD0F) = 0x51;         /* B: count A underflows, force load, start */
    REG(0xDD0E) = 0x11;         /* A: phi2, continuous, force load, start */
}

static unsigned long tstop(void)
{
    unsigned a, b;
    REG(0xDD0E) = 0; REG(0xDD0F) = 0;
    a = 0xFFFFu - (REG(0xDD04) | (unsigned)REG(0xDD05) << 8);
    b = 0xFFFFu - (REG(0xDD06) | (unsigned)REG(0xDD07) << 8);
    return ((unsigned long)b << 16) + a;
}

static unsigned long timed(int16_t x0, int16_t y0, int16_t x1, int16_t y1)
{
    unsigned long t;
    SEI();
    tstart();
    gfx_line(x0, y0, x1, y1);
    t = tstop();
    CLI();
    return t - t_cal;
}

static void regs(void)
{
    r_dd00 = REG(0xDD00) & 3; r_d011 = REG(0xD011) & 0x7F; r_d016 = REG(0xD016);
    r_d018 = REG(0xD018); r_d020 = REG(0xD020) & 15; r_d021 = REG(0xD021) & 15;
}

static void check(int ok, const char *what)
{
    if (!ok) { ++failures; out_str("bad: "); out_line(what); }
}

int main(void)
{
    unsigned i;
    STEP = 0; ACK = 0;
    io_text_open();
    out_line("gfx check (c64)");
    out_line("text before the drawing");
    gfx_close();                /* closing before any open must do nothing */
    regs();
    SEI(); tstart(); t_cal = tstop(); CLI();
    memcpy(screen, (void *)0x0400, 1000);
    memcpy(colours, (void *)0xD800, 1000);
    sync(1);

    gfx_open();
    sync(2);

    for (i = 0; i < GFX_NLINES; i++)
        gfx_line(gfx_lines[i][0], gfx_lines[i][1], gfx_lines[i][2], gfx_lines[i][3]);
    /* timings: these lines repeat pixels of gfx_lines.h, so the bitmap is
     * the same with or without them */
    t_diag  = timed(0, 0, 319, 199);              /* 320 pixels, x-major */
    t_horiz = timed(10, 10, 309, 10);             /* 300 pixels, horizontal */
    t_steep = timed(319, 0, 319, 199);            /* 200 pixels, vertical */
    t_far   = timed(GL_MIN, GL_MIN, 32767, 32767);/* 200 pixels after clipping */
    t_dot   = timed(160, 100, 160, 100);          /* 1 pixel: the fixed cost */
    /* the VIC registers while the bitmap is shown, for check_bitmap.py */
    REG(0x02A9) = REG(0xDD00); REG(0x02AA) = REG(0xD011); REG(0x02AB) = REG(0xD016);
    REG(0x02AC) = REG(0xD018); REG(0x02AD) = REG(0xD020);
    sync(3);

    gfx_close();
    check((REG(0xDD00) & 3) == r_dd00, "dd00");
    check((REG(0xD011) & 0x7F) == r_d011, "d011");
    check(REG(0xD016) == r_d016, "d016");
    check(REG(0xD018) == r_d018, "d018");
    check((REG(0xD020) & 15) == r_d020, "d020");
    check((REG(0xD021) & 15) == r_d021, "d021");
    check(memcmp(screen, (void *)0x0400, 1000) == 0, "text screen");
    for (i = 0; i < 1000; i++)
        if ((colours[i] ^ REG(0xD800 + i)) & 15) break;
    check(i == 1000, "colour ram");
    gfx_open();                 /* a second drawing: open again, close again */
    gfx_line(0, 0, 319, 199);
    gfx_close();
    check(REG(0xD018) == r_d018 && (REG(0xDD00) & 3) == r_dd00, "reopen");

    out_str("cycles: cal "); out_ul(t_cal); out_nl();
    out_str("diag 320px "); out_ul(t_diag); out_nl();
    out_str("horiz 300px "); out_ul(t_horiz); out_nl();
    out_str("vert 200px "); out_ul(t_steep); out_nl();
    out_str("far 200px "); out_ul(t_far); out_nl();
    out_str("dot 1px "); out_ul(t_dot); out_nl();
    if (t_diag > 49000UL || t_horiz > 49000UL) check(0, "300-pixel line over 50 ms");
    if (failures == 0) out_line("PASS");
    else { out_str("FAIL "); out_ul(failures); out_nl(); }
    for (;;) ;
    return 0;
}
