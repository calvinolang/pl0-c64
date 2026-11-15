/* core_check.c -- the real compiler and VM (all of src/core) on the C64.
 *
 * Linked with src/core, num_c64, gfx_c64 and io_c64 and the link config of
 * the final program. Compiled with the ASCII charmap like every C64 source
 * (Makefile), so the programs below are ASCII, as the source buffer will
 * be. Every program is copied into the 8 KB source buffer under the BASIC
 * ROM (BASICBSS, $A000) and compiled from there, as the editor will do.
 * Errors print through err_message, the text every front end shows.
 * After every run the source buffer must still hold the program.
 *
 * Two builds (the wrapper Makefile writes includes a generated header
 * first, tools/pl0_to_c.py: core_src[], CORE_SRC_NAME, CORE_SRC_DRAWING):
 *
 * `make c64-core` (core_src = examples/square.pl0): runs the programs
 * below; each prints on one labelled screen line, and
 * tools/check_c64_run.py --screen tests/c64/core_check.expected compares
 * the whole screen line by line. (g) loops until the harness types
 * RUN/STOP after the line `g: stop me`. (h) draws the square; the harness
 * saves RAM at --snap step 1 while the bitmap is shown and compares it
 * with the host renderer (pixel count, bounding box).
 *
 * `make c64-example EX=name` (CORE_EXAMPLE defined, core_src = the
 * example): clears the screen, runs the example (its text output stays on
 * rows 0..21: the example rules allow 22 lines of at most 39 characters),
 * saves a drawing at --snap step 1, then prints `cycles C R` (CPU cycles
 * of pl0_compile and of pl0_run, from the CIA2 timers: about 985248 per
 * second on a PAL C64) and `DONE`. */
#include <string.h>
#include <stdint.h>
#include "core/compiler.h"
#include "core/vm.h"
#include "core/turtle.h"
#include "core/errors.h"
#include "platform/io.h"
#include "platform/io_c64.h"
#include "platform/gfx.h"
#include "out.h"

#define REG(a) (*(volatile uint8_t *)(a))
#define STEP REG(0x02A7)
#define ACK  REG(0x02A8)

/* The editor's source buffer: RAM under the BASIC ROM, never passed to
 * the ROM (the lexer copies number text to low RAM before num_parse). */
#pragma bss-name (push, "BASICBSS")
static char source[MAX_SOURCE];
#pragma bss-name (pop)

static Program prog;
static PL0Error e;
static char msg[ERR_MESSAGE_SIZE];
static unsigned long t_compile, t_run;
static uint8_t step;

/* CIA2 timer A counts CPU cycles from $FFFF, timer B its underflows: a
 * 32-bit down counter. Interrupts may stay on (the timers never raise an
 * NMI: $DD0D is cleared below), so the count includes the KERNAL IRQ, as a
 * user would see it. */
static void tstart(void)
{
    REG(0xDD0D) = 0x7F;
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

static void show_error(void)
{
    err_message(msg, e.code, e.line);
    out_line(msg);
}

/* Compile and run text from the source buffer. A drawing is saved by the
 * harness (--snap) while it is shown, then the text screen comes back.
 * Returns 0, or 1 after printing the error message. */
static int run(const char *text)
{
    int rc;
    size_t n = strlen(text);
    if (n >= MAX_SOURCE) {
        out_line("FAIL source too long");
        return 1;
    }
    memcpy(source, text, n + 1);
    tstart();
    rc = pl0_compile(source, &prog, &e);
    t_compile = tstop();
    t_run = 0;
    if (!rc) {
        tstart();
        rc = pl0_run(&prog, &e);
        t_run = tstop();
    }
    if (turtle_used()) {
        STEP = ++step;          /* harness: save RAM, then ACK */
        while (ACK != step) ;
        gfx_close();
    }
    if (rc) show_error();
    /* the source must survive a run and a drawing (Global Constraints) */
    if (memcmp(source, text, n + 1) != 0) {
        out_line("FAIL source buffer changed");
        return 1;
    }
    return rc;
}

#ifndef CORE_EXAMPLE

/* label, the program's output, a new line (an error ends its own line) */
static void line(const char *label, const char *text)
{
    out_str(label);
    out_str(" ");
    if (!run(text)) out_nl();
}

int main(void)
{
    STEP = 0; ACK = 0;
    io_text_open();
    out_line("core_check (c64)");
    line("a", "!1+2*3.");
    /* upper and mixed case: keywords and names are case-insensitive */
    line("b", "VAR R;\n"
              "PROCEDURE Fact(N);\n"
              "BEGIN\n"
              "  IF N <= 1 THEN R := 1;\n"
              "  IF N > 1 THEN BEGIN CALL Fact(N - 1); R := R * N END\n"
              "END;\n"
              "BEGIN CALL FACT(10); !r END.");
    line("c", "!x.");
    line("d", "var a;\nbegin\n  a := 0;\n  !1 / a\nend.");
    line("e1", "!sin(1).");
    line("e2", "!sqrt(2).");
    line("e3", "!4*atan(1).");
    line("f1", "var s;\n"
               "procedure sum(n);\n"
               "begin if n > 0 then begin call sum(n - 1); s := s + n end end;\n"
               "begin s := 0; call sum(40); !s end.");
    out_line("f2");                      /* the message is 39 characters */
    run("procedure r;\nbegin call r end;\ncall r.");
    line("f3", "!40*41/2.");             /* still works after the overflow */
    out_line("g: stop me");              /* the harness types RUN/STOP */
    line("g", "var i; begin i := 0; while 1 = 1 do i := i + 1 end.");
    out_str("h ");
    if (!run((const char *)core_src)) out_line("drawn");
    out_str("DONE");
    for (;;) ;
    return 0;
}

#else

int main(void)
{
    STEP = 0; ACK = 0;
    io_text_open();
    run((const char *)core_src);
    out_nl();
    out_str("cycles ");
    out_ul(t_compile);
    out_str(" ");
    out_ul(t_run);
    out_nl();
    out_str("DONE");
    for (;;) ;
    return 0;
}

#endif
