/* main.c -- the PL/0 program for the C64 (build/pl0.prg, `make c64`).
 *
 * Starts in the editor with the square example loaded (named SQUARE), so
 * the first key can be F1. F1 compiles the text; an error goes back to the
 * editor with the cursor on its line and the message on row 24. Running a
 * compiled program, opening and saving come with Task 15: run_screen,
 * open_screen and save_screen below are the hooks it replaces.
 *
 * The starter text comes from examples/square.pl0 through a header the
 * Makefile generates (tools/pl0_to_c.py: core_src[], ASCII bytes as
 * numbers). Compiled with the ASCII charmap like every C64 source.
 * PL0_SOURCE_CAP (test builds only: make c64-editor builds
 * build/c64/pl0_tiny.prg with 400) shrinks the source buffer so that a
 * test can fill it quickly. */
#include <stdint.h>
#include <string.h>
#include "core/ascii.h"     /* the texts here are ASCII (charmap guard) */
#include "core/compiler.h"
#include "c64/textbuf.h"
#include "c64/editor.h"

#ifndef PL0_SOURCE_CAP
#define PL0_SOURCE_CAP MAX_SOURCE
#endif

/* The source text: RAM under the BASIC ROM ($A000, BASICBSS), never
 * handed to the ROM float shims (the lexer copies number text to low RAM
 * first). */
#pragma bss-name (push, "BASICBSS")
static char source[PL0_SOURCE_CAP];
#pragma bss-name (pop)

/* HIBSS ($C000-$CBFF, src/c64/pl0-c64.cfg) holds the editor's variables;
 * crt0 clears only the ordinary BSS, so main clears this area first. */
extern char _HIBSS_RUN__[], _HIBSS_SIZE__[];

static TextBuf text;
static Program prog;
static PL0Error err;

/* TODO(Task 15): run prog (text screen, drawing, any key back) and return
 * a runtime error through err. For now a compiled program only reports
 * success on row 24. */
static const PL0Error *run_screen(const Program *p)
{
    (void)p;
    editor_set_message("OK - COMPILED (run comes next)");
    return 0;
}

/* TODO(Task 15): the example picker (F3) and saving to disk (F5). */
static void open_screen(void) { editor_set_message("F3 OPEN: NOT YET"); }
static void save_screen(void) { editor_set_message("F5 SAVE: NOT YET"); }

int main(void)
{
    const PL0Error *e = 0;
    EdAction a;

    memset(_HIBSS_RUN__, 0, (size_t)_HIBSS_SIZE__);
    tb_init(&text, source, sizeof source);
    tb_set(&text, (const char *)core_src);
    editor_set_name("SQUARE");
    for (;;) {
        a = editor_run(&text, e);
        e = 0;
        if (a == ED_RUN) {
            if (pl0_compile(tb_text(&text), &prog, &err)) e = &err;
            else e = run_screen(&prog);
        } else if (a == ED_OPEN) {
            open_screen();
        } else {
            save_screen();
        }
    }
    return 0;
}
