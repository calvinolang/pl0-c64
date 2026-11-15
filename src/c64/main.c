/* main.c -- the PL/0 program for the C64 (build/pl0.prg, `make c64`).
 *
 * Starts in the editor with the square example loaded (named SQUARE), so
 * the first key can be F1. The editor returns for:
 *   F1  compile; an error goes back to the editor with the cursor on its
 *       line. Otherwise run (run_program): output on the text screen, the
 *       first turtle statement switches to the bitmap. At the end:
 *       runtime error (RUN/STOP too: `stopped`): the text view with the
 *       error after the output and `-- PRESS ANY KEY --`, then the editor
 *       with the error shown; no drawing: `-- PRESS ANY KEY --`; a
 *       drawing stays on screen, SPACE toggles drawing and text, any other
 *       key returns.
 *   F3  (after the editor's discard question) the picker (picker.c), then
 *       load the chosen file.
 *   F5  save: `SAVE AS: NAME` on row 24, then disk.c.
 * docs/architecture/editor.md describes the screens and keys.
 *
 * Memory: `big` is the compiled Program (4,741 bytes) or, while no
 * program is compiled or run, the picker's list. Ownership rule: only
 * run_program uses big.prog (compile, then run, all inside it) and only
 * open_program uses big.list (picker_run fills it, the chosen name is
 * copied out before picker_run returns); both are called from the loop in
 * main one at a time, so neither ever sees the other's data, and nothing
 * in big outlives the call that wrote it.
 *
 * The starter text comes from examples/square.pl0 through a header the
 * Makefile generates (tools/pl0_to_c.py: core_src[], ASCII bytes as
 * numbers). Compiled with the ASCII charmap like every C64 source; PETSCII
 * key codes are numbers. PL0_SOURCE_CAP (test builds only: make
 * c64-editor builds build/c64/pl0_tiny.prg with 400) shrinks the source
 * buffer so that a test can fill it quickly. */
#include <stdint.h>
#include <string.h>
#include <cbm.h>
#include "core/ascii.h"     /* the texts here are ASCII (charmap guard) */
#include "core/compiler.h"
#include "core/vm.h"
#include "core/turtle.h"
#include "platform/io.h"
#include "platform/io_c64.h"
#include "platform/gfx.h"
#include "platform/gfx_c64.h"
#include "c64/textbuf.h"
#include "c64/editor.h"
#include "c64/disk.h"
#include "c64/picker.h"

#ifndef PL0_SOURCE_CAP
#define PL0_SOURCE_CAP MAX_SOURCE
#endif

#define REG(a)  (*(volatile uint8_t *)(a))
#define NDX     REG(0xC6)       /* keys in the KERNAL keyboard buffer */
#define C_INFO  1               /* white */
#define C_ASK   7               /* yellow */
#define NAME_IN 16              /* characters the SAVE AS field takes */

/* The source text: RAM under the BASIC ROM ($A000, BASICBSS), never
 * handed to the ROM float shims (the lexer copies number text to low RAM
 * first). */
#pragma bss-name (push, "BASICBSS")
static char source[PL0_SOURCE_CAP];
#pragma bss-name (pop)

/* HIBSS ($C000-$CBFF, src/c64/pl0-c64.cfg) holds the editor's variables
 * and the VM stack; crt0 clears only the ordinary BSS, so main clears this
 * area first. */
extern char _HIBSS_RUN__[], _HIBSS_SIZE__[];
/* LOWBSS (the cassette buffer $033C-$03FB) holds the small variables of
 * this file, disk.c/disk.s and picker.c/picker.s; main clears it too. */
extern char _LOWBSS_RUN__[], _LOWBSS_SIZE__[];

#define PICK_MAX (sizeof(Program) / PICK_ROW)
static union {
    Program prog;
    char list[PICK_MAX * PICK_ROW];
} big;
typedef char pick_max_fits[256 - PICK_MAX];     /* PICK_MAX <= 255 */

static TextBuf text;
#pragma bss-name (push, "LOWBSS")
static PL0Error err;
static char msg[ERR_MESSAGE_SIZE];  /* run screen line, editor messages */
static char fname[NAME_IN + 1];     /* the SAVE AS field, the chosen file */
static uint8_t k, len;
#pragma bss-name (pop)

/* the next key (PETSCII), waiting for one */
static uint8_t get_key(void)
{
    do k = cbm_k_getin(); while (k == 0);
    return k;
}

/* the same after dropping the keys typed so far (the run screen: keys
 * typed while the program ran are not an answer to its last prompt) */
static uint8_t wait_key(void)
{
    NDX = 0;
    return get_key();
}

/* a line of the run screen, starting on a new row */
static void run_line(const char *s)
{
    if (io_col) io_put_nl();
    io_put_str(s);
}

/* F1 */
static const PL0Error *run_program(void)
{
    uint8_t shown = 1;
    if (pl0_compile(tb_text(&text), &big.prog, &err)) return &err;
    io_text_open();
    NDX = 0;
    if (pl0_run(&big.prog, &err)) {
        gfx_close();
        err_message(msg, err.code, err.line);
        run_line(msg);
        io_put_nl();
        io_put_str("-- PRESS ANY KEY --");
        wait_key();
        return &err;
    }
    if (!turtle_used()) {
        run_line("-- PRESS ANY KEY --");
        wait_key();
        return 0;
    }
    run_line("-- SPACE: DRAWING  ANY KEY: EDITOR --");
    while (wait_key() == ' ') {
        shown ^= 1;
        if (shown) gfx_show();
        else gfx_close();
    }
    gfx_close();
    return 0;
}

/* `q` on row 24 until Y (1) or N / RUN/STOP (0) */
static uint8_t ask(const char *q)
{
    ui_bar(q, C_ASK);
    for (;;) {
        get_key();
        if (k == 0x59 || k == 0xD9) return 1;               /* y, Y */
        if (k == 0x4E || k == 0xCE || k == 0x03) return 0;  /* n, N, STOP */
    }
}

/* msg = a b */
static void join(const char *a, const char *b)
{
    strcpy(msg, a);
    strcat(msg, b);
}

/* F5 */
static void save_program(void)
{
    uint8_t rc;
    strcpy(fname, editor_name());
    len = (uint8_t)strlen(fname);
    for (;;) {
        join("SAVE AS: ", fname);
        ui_bar(msg, C_INFO);
        REG(0x07C0 + 9 + len) = 0x20;          /* the cursor: a plain cell */
        get_key();
        if (k == 0x0D) break;                               /* RETURN */
        if (k == 0x03) return;                              /* RUN/STOP */
        if (k == 0x14) {                                    /* DEL */
            if (len) fname[--len] = 0;
            continue;
        }
        if (k >= 0xC1 && k <= 0xDA) k -= 0x80;              /* shifted A-Z */
        if (k >= 0x20 && k <= 0x5F && len < NAME_IN) {      /* typed as is */
            fname[len++] = (char)k;
            fname[len] = 0;
        }
    }
    if (!disk_name_ok(fname)) {
        editor_set_alert("BAD NAME");
        return;
    }
    ui_bar("SAVING...", C_INFO);
    rc = disk_save(fname, &text, 0);
    if (rc == DK_EXISTS) {
        if (!ask("REPLACE? (Y/N)")) {
            editor_set_message("NOT SAVED");
            return;
        }
        ui_bar("SAVING...", C_INFO);
        rc = disk_save(fname, &text, 1);
    }
    if (rc) {
        editor_set_alert(disk_message(rc));
        return;
    }
    editor_set_name(fname);
    editor_clear_modified();
    join("SAVED ", fname);
    editor_set_message(msg);
}

/* F3 */
static void open_program(void)
{
    uint8_t rc = picker_run(big.list, (uint8_t)PICK_MAX, fname);
    if (rc == PK_BACK) return;
    if (rc == PK_EMPTY) {
        editor_set_message("NO PROGRAMS");
        return;
    }
    if (rc == 0) {
        join("LOADING ", fname);
        ui_bar(msg, C_INFO);
        rc = disk_load(fname, &text);
        if (rc == 0) {
            editor_set_name(fname);
            editor_clear_modified();
            return;
        }
        if (disk_touched) {     /* part of the file: not the old program */
            editor_set_name("");
            editor_set_modified(1);
        }
    }
    if (rc == DK_NODEV || rc == 74 || rc == 21) editor_set_alert("NO DISK");
    else editor_set_alert(disk_message(rc));
}

int main(void)
{
    const PL0Error *e = 0;
    EdAction a;

    memset(_HIBSS_RUN__, 0, (size_t)_HIBSS_SIZE__);
    memset(_LOWBSS_RUN__, 0, (size_t)_LOWBSS_SIZE__);
    disk_init();
    tb_init(&text, source, sizeof source);
    tb_set(&text, (const char *)core_src);
    editor_set_name("SQUARE");
    for (;;) {
        a = editor_run(&text, e);
        e = 0;
        if (a == ED_RUN) e = run_program();
        else if (a == ED_OPEN) open_program();
        else save_program();
    }
    return 0;
}
