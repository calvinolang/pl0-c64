/* editor.c -- the C64 editor screen (editor.h; keys in
 * docs/architecture/editor.md).
 *
 * Screen: row 0 status bar (reverse), rows 1-23 the text, one buffer line
 * per row, row 24 the key bar or a message (reverse). Only the cursor's row
 * scrolls horizontally (`hs` characters hidden, `<` in column 0); a line
 * longer than the window shows `>` in column 39. The cursor is a steady
 * reverse-video cell (steady, so a screen dump shows exactly one).
 *
 * Encodings: the buffer is ASCII. Keys arrive in PETSCII from the KERNAL
 * keyboard buffer and are converted in key_ascii(); the screen gets screen
 * codes through ed_tbl (filled from io_scr, io_c64.s). Like every C64
 * source this file is compiled with the ASCII charmap (Makefile wrapper),
 * so its string literals are ASCII; PETSCII codes are written as numbers.
 *
 * Speed: the textbuf scanning helpers are O(buffer), so the editor caches
 * the cursor's line number and line start (`line`, `lstart`) and the top
 * line of the view and its offset (`top`, `toff`), and updates them on
 * every key from local information (one line scanned at most); the view
 * follows the cursor after every key, so every edit is on a visible line.
 * Keys waiting in the keyboard buffer are all handled before the screen is
 * drawn again, so a redraw must cover every row those keys changed:
 * D_ROW marks one edited row (`rline`), a second edited row or a new,
 * joined or split line turns that into D_ALL from the first changed line
 * down (`fline`); a scroll redraws all 23 rows. Drawing: ed_rows/ed_put
 * (edscreen.s).
 *
 * cc65 notes: declarations at block start; no `?:` (the -O ternary bug,
 * spike-results.md section 9: none is used here) and no subtraction of
 * unsigned chars cast to int16_t (all arithmetic is uint16_t); file
 * statics instead of locals (smaller code). */
#include <stddef.h>
#include <string.h>
#include <cbm.h>
#include "core/ascii.h"
#include "c64/editor.h"

/* All uninitialised statics of this file (and of edscreen.s) live in HIBSS
 * ($C000-$CBFF, src/c64/pl0-c64.cfg), which crt0 does not clear: main()
 * clears it before anything else. Low RAM up to $97FF is full. */
#pragma bss-name (push, "HIBSS")

#define REG(a)  (*(volatile uint8_t *)(a))
#define SCREEN  ((uint8_t *)0x0400)
#define COLRAM  ((uint8_t *)0xD800)
#define NDX     REG(0xC6)       /* keys in the KERNAL keyboard buffer */
#define BLNSW   REG(0xCC)       /* KERNAL cursor blink off when nonzero */
#define TEXTCOL REG(0x0286)     /* KERNAL text colour (io_c64.s uses it) */
#define RPTFLG  REG(0x028A)     /* $80: every key repeats */
#define MODE    REG(0x0291)     /* $80: SHIFT+C= does not switch charsets */

#define TEXTROWS 23             /* rows 1..23 */
#define C_TEXT   14             /* light blue (on blue): the C64 default */
#define C_ERROR  10             /* light red: errors, BUFFER FULL */
#define C_INFO   1              /* white: other messages */
#define C_ASK    7              /* yellow: the question of F2 */

/* PETSCII key codes */
#define K_STOP   0x03
#define K_RETURN 0x0D
#define K_SRET   0x8D           /* SHIFT+RETURN */
#define K_DOWN   0x11
#define K_UP     0x91
#define K_RIGHT  0x1D
#define K_LEFT   0x9D
#define K_HOME   0x13
#define K_CLR    0x93           /* SHIFT+HOME */
#define K_DEL    0x14
#define K_INST   0x94           /* SHIFT+DEL */
#define K_F1     0x85
#define K_F3     0x86
#define K_F5     0x87
#define K_F7     0x88
#define K_F2     0x89           /* SHIFT+F1 */

/* what needs drawing */
#define D_ROW 1                 /* the row of line rline */
#define D_ALL 2                 /* the text rows from line fline down */
#define D_BAR 4                 /* row 24 */
#define FULL() (fline = 0, dirty |= D_ALL)

/* edscreen.s */
extern uint8_t *ed_scr;
extern uint16_t ed_hs;
extern uint8_t ed_rev;
extern uint8_t ed_tbl[256];
extern uint8_t ed_n, ed_cur;
void __fastcall__ ed_rows(uint16_t off);
uint16_t __fastcall__ ed_bol(uint16_t off);
uint16_t __fastcall__ ed_eol(uint16_t off);
void __fastcall__ ed_put(const char *s);
/* io_c64.s */
uint8_t __fastcall__ io_scr(uint8_t c);

static const char bar[] = " F1 Run F2 New F3 Open F5 Save F7 Help";

static const char *const help[] = {
    " PL/0 HELP",
    "",
    "  F1          compile and run",
    "  F2          new program (SHIFT+F1)",
    "  F3          open an example or program",
    "  F5          save to disk",
    "  F7          this help",
    "  RUN/STOP    stop a running program",
    "",
    "  CRSR keys   move the cursor",
    "  CLR/HOME    start of the line",
    "  SHIFT+HOME  end of the line",
    "  INST/DEL    delete to the left",
    "  SHIFT+DEL   delete to the right",
    "  RETURN      new line",
    "",
    " THE LANGUAGE (upper or lower case)",
    "  var x;  begin x := 7; !x*x end.",
    "  !x prints, ?x reads, ! new line",
    "  if c then s  while c do s",
    "  forward(n) turn(deg) penup pendown",
    "  [ comment ]  (SHIFT+: and SHIFT+;)",
    " AFTER A RUN: SPACE drawing/text,",
    "  any other key back to the editor",
    " PRESS ANY KEY TO RETURN"
};

/* The caller's TextBuf while editing (copied back on return): a static
 * object, so cc65 reaches its fields with absolute addressing, and
 * edscreen.s reads buf, cap, len and gs at the offsets checked here. */
TextBuf ed_tb;
#define T ed_tb
typedef char tb_layout_check[1 - 2 * ((offsetof(TextBuf, buf) ^ 0) |
    (offsetof(TextBuf, cap) ^ 2) | (offsetof(TextBuf, len) ^ 4) |
    (offsetof(TextBuf, gs) ^ 8))];

static uint16_t line;           /* cursor line, 1-based */
static uint16_t lstart;         /* offset of its first character */
static uint16_t top;            /* line shown on row 1 */
static uint16_t toff;           /* offset of its first character */
static uint16_t hs;             /* characters of the cursor's line hidden */
static uint16_t want;           /* sticky column (0-based) + 1; 0: none */
static uint16_t dline, dhs;     /* line and hs of the row last drawn */
static uint16_t fline, foff;    /* D_ALL: redraw from this line (its
                                   offset) down; fline 0: from the top */
static uint16_t rline, rstart;  /* D_ROW: the edited line, its offset */
static uint8_t *curs;           /* screen cell showing the cursor, or 0 */
static uint8_t dirty;
static uint8_t modified;
static char name[EDITOR_NAME_MAX + 1];
static const char *msg;         /* message on row 24, or 0: the key bar */
static uint8_t msgcol;
static char msgbuf[ERR_MESSAGE_SIZE];
static char sbuf[41];           /* the status bar */
static uint8_t tbl_ready;
static uint8_t old_rpt, old_mode;

/* ---- accessors (Task 15) */

void editor_set_name(const char *s)
{
    name[0] = 0;
    if (s) {
        strncpy(name, s, EDITOR_NAME_MAX);
        name[EDITOR_NAME_MAX] = 0;
    }
}
const char *editor_name(void) { return name; }
void editor_set_modified(uint8_t m) { modified = m; }
void editor_set_message(const char *s)
{
    msg = s;
    msgcol = C_INFO;
}
void editor_set_alert(const char *s)
{
    msg = s;
    msgcol = C_ERROR;
}

/* the text changed from line l (starting at offset off) down */
static void redraw_from(uint16_t l, uint16_t off)
{
    if (!(dirty & D_ALL) || l < fline) {
        fline = l;
        foff = off;
    }
    dirty |= D_ALL;
}

/* the cursor's line changed within itself */
static void touch_row(void)
{
    if (!(dirty & D_ROW)) {
        rline = line;
        rstart = lstart;
        dirty |= D_ROW;
    } else if (rline != line) {         /* a second row: both, and between */
        redraw_from(rline, rstart);
        redraw_from(line, lstart);
    }
}

static void show(const char *s, uint8_t colour)
{
    msg = s;
    msgcol = colour;
    dirty |= D_BAR;
}

/* ---- keyboard */

/* Next key (PETSCII), waiting for one. */
static uint8_t key_wait(void)
{
    uint8_t k;
    do k = cbm_k_getin(); while (k == 0);
    return k;
}

/* PETSCII key -> ASCII character for the text, or 0 if the key types
 * nothing: unshifted letters are lower case, shifted ones upper case (the
 * language ignores case); SHIFT+: and SHIFT+; ([ and ] on the keys) type
 * the comment braces { }, which the screen shows as [ ]; SHIFT+SPACE is a
 * space. Colours, graphics and other control codes give 0. */
static uint8_t key_ascii(uint8_t k)
{
    if (k >= 0x41 && k <= 0x5A) return (uint8_t)(k + 0x20);
    if (k >= 0xC1 && k <= 0xDA) return (uint8_t)(k - 0x80);
    if (k == 0x5B) return '{';
    if (k == 0x5D) return '}';
    if (k == 0xA0) return ' ';
    if (k >= 0x20 && k <= 0x5F) return k;   /* digits, punctuation, @ ^ _ */
    return 0;
}

/* ---- buffer helpers
 * The gap buffer's own scans (tb_up, tb_down, tb_home, tb_end, tb_line)
 * read every character through tb_at, about 1000 cycles each in cc65
 * code (measured: 57 ms for a 57-character line). The editor scans with
 * ed_bol/ed_eol (edscreen.s, about 40 cycles a character) and keeps the
 * sticky column itself (`want`), with the rule of tb_up/tb_down: the
 * column of the first vertical move, clamped to each line's length. */

static uint8_t *row_addr(uint8_t r)
{
    return SCREEN + (uint16_t)r * 40;
}

/* ---- drawing */

static const uint16_t pow10[] = { 10000, 1000, 100, 10, 1 };

/* decimal digits of v at d, no leading zeros; returns the end (statics,
 * not parameters: cc65 keeps parameters on its slow software stack) */
static uint16_t nv;
static char *put_num(char *d)
{
    static uint8_t i, dig, any;
    static uint16_t pw;
    any = 0;
    for (i = 0; i < 5; ++i) {
        pw = pow10[i];
        dig = '0';
        while (nv >= pw) {
            nv -= pw;
            ++dig;
        }
        if (dig != '0' || any || i == 4) {
            *d++ = (char)dig;
            any = 1;
        }
    }
    return d;
}

static void draw_status(void)
{
    static const char *s;
    static char *d;
    static uint8_t i;
    memset(sbuf, ' ', 40);
    sbuf[40] = 0;
    memcpy(sbuf + 1, "PL/0", 4);
    s = name;
    if (!*s) s = "NEW";
    d = sbuf + 8;
    for (i = 0; i < 12 && s[i]; ++i) *d++ = s[i];
    if (modified) *d = '*';
    d = sbuf + 24;
    *d++ = 'L';
    *d++ = ' ';
    nv = line;
    d = put_num(d);
    memcpy(d, "  C ", 4);
    nv = (uint16_t)(T.cur - lstart + 1);
    put_num(d + 4);
    ed_scr = SCREEN;
    ed_rev = 0x80;
    ed_put(sbuf);
    ed_rev = 0;
}

/* Keep the cursor's line on rows 1..23. Moves of one line scan one line;
 * editor_run places the view itself for long jumps. */
static void follow_v(void)
{
    if (line < top) {
        top = line;
        toff = lstart;
        FULL();
    }
    while (line > (uint16_t)(top + (TEXTROWS - 1))) {
        toff = (uint16_t)(ed_eol(toff) + 1);    /* a later line exists */
        ++top;
        FULL();
    }
}

/* Horizontal scroll of the cursor's row: the cursor stays in columns
 * 1..38 (0..38 when nothing is hidden), so it never sits under a marker;
 * the window moves only as far as needed, or back to 0 when that shows the
 * cursor. */
static void follow_h(void)
{
    uint16_t c = (uint16_t)(T.cur - lstart);
    if (hs != 0 && c <= hs) {
        if (c <= 38) hs = 0;
        else hs = (uint16_t)(c - 1);
    }
    if (c > (uint16_t)(hs + 38)) hs = (uint16_t)(c - 38);
}

static void render(void)
{
    follow_v();
    follow_h();
    if (line != dline) {
        if (dhs) FULL();                /* the old row scrolls back */
        if (hs) touch_row();
    } else if (hs != dhs) {
        touch_row();
    }
    if (curs) *curs &= 0x7F;
    ed_n = 0;
    if (dirty & D_ALL) {
        if (fline <= top) {
            fline = top;
            foff = toff;
        }
        if ((dirty & D_ROW) && rline < fline) {
            fline = rline;
            foff = rstart;
        }
        if (fline <= (uint16_t)(top + (TEXTROWS - 1)))
            ed_n = (uint8_t)(top + TEXTROWS - fline);
    } else if (dirty & D_ROW) {
        fline = rline;
        foff = rstart;
        ed_n = 1;
    }
    if (ed_n) {
        ed_scr = row_addr((uint8_t)(fline - top + 1));
        ed_hs = hs;
        ed_cur = 0xFF;
        if (line >= fline) ed_cur = (uint8_t)(line - fline);
        ed_rows(foff);
    }
    curs = row_addr((uint8_t)(line - top + 1)) + (uint16_t)(T.cur - lstart - hs);
    *curs |= 0x80;
    if (dirty & D_BAR) {
        ed_scr = row_addr(24);
        ed_rev = 0x80;
        if (msg) {
            ed_put(msg);
            memset(COLRAM + 960, msgcol, 40);
        } else {
            ed_put(bar);
            memset(COLRAM + 960, C_TEXT, 40);
        }
        ed_rev = 0;
    }
    draw_status();                      /* last: a complete frame for tests */
    dline = line;
    dhs = hs;
    dirty = 0;
}

static void help_screen(void)
{
    static uint8_t r;
    for (r = 0; r < 25; ++r) {
        ed_scr = row_addr(r);
        ed_rev = 0;
        if (r == 0 || r == 24) ed_rev = 0x80;
        if (r < sizeof help / sizeof help[0]) ed_put(help[r]);
        else ed_put("");
    }
    ed_rev = 0;
    memset(COLRAM + 960, C_TEXT, 40);
    key_wait();
    curs = 0;
    dirty = D_BAR;
    FULL();
}

/* ---- editing keys */

static void k_insert(uint8_t c)
{
    if (tb_insert(&T, (char)c)) {
        show("BUFFER FULL", C_ERROR);
        return;
    }
    modified = 1;
    if (c == '\n') {
        redraw_from(line, lstart);
        ++line;
        lstart = T.cur;
    } else {
        touch_row();
    }
}

static void k_backspace(void)
{
    if (T.cur == 0) return;
    modified = 1;
    if (T.cur == lstart) {            /* joins this line to the one above */
        tb_backspace(&T);
        --line;
        lstart = ed_bol(T.cur);
        redraw_from(line, lstart);
    } else {
        tb_backspace(&T);
        touch_row();
    }
}

static void k_delete(void)
{
    if (T.cur == T.len) return;
    modified = 1;
    if (tb_at(&T, T.cur) == '\n') redraw_from(line, lstart);  /* joins */
    else touch_row();
    tb_delete(&T);
}

static void k_left(void)
{
    if (T.cur == 0) return;
    if (T.cur == lstart) {
        tb_left(&T);
        --line;
        lstart = ed_bol(T.cur);
    } else {
        tb_left(&T);
    }
}

static void k_right(void)
{
    if (T.cur == T.len) return;
    if (tb_at(&T, T.cur) == '\n') {
        tb_right(&T);
        ++line;
        lstart = T.cur;
    } else {
        tb_right(&T);
    }
}

/* cursor to column want-1 of the line [s, e], or its end */
static void to_col(uint16_t s, uint16_t e)
{
    if (want == 0) want = (uint16_t)(T.cur - lstart + 1);
    lstart = s;
    s += (uint16_t)(want - 1);
    if (s > e) s = e;
    T.cur = s;
}

static void k_up(void)
{
    uint16_t s;
    if (lstart == 0) return;            /* line 1 */
    s = ed_bol((uint16_t)(lstart - 1));
    to_col(s, (uint16_t)(lstart - 1));
    --line;
}

static void k_down(void)
{
    uint16_t e = ed_eol(T.cur);
    if (e == T.len) return;             /* last line */
    ++e;
    to_col(e, ed_eol(e));
    ++line;
}

static void clear_text(void)
{
    tb_set(&T, "");
    name[0] = 0;
    modified = 0;
    line = top = 1;
    lstart = toff = hs = 0;
    FULL();
}

/* F2 and F3: a modified, non-empty text is discarded only after Y;
 * returns nonzero if it may go */
static uint8_t discard_ok(void)
{
    uint8_t k;
    if (T.len == 0 || !modified) return 1;
    show("DISCARD PROGRAM? (Y/N)", C_ASK);
    render();
    for (;;) {
        k = key_wait();
        if (k == 0x59 || k == 0xD9) break;                  /* y, Y */
        if (k == 0x4E || k == 0xCE || k == K_STOP) {        /* n, N */
            show(0, C_TEXT);
            return 0;
        }
    }
    show(0, C_TEXT);
    return 1;
}

/* ---- entry and exit */

/* line number, line start and view for the cursor; the text may be new
 * (one pass over the text before the cursor, about 40 cycles a
 * character: 0.3 s for 8 KB) */
static void place(const PL0Error *err)
{
    static uint16_t n, o;
    if (err) {                          /* cursor to the start of err->line */
        o = 0;
        for (n = err->line; n > 1; --n) {
            o = ed_eol(o);
            if (o == T.len) break;      /* fewer lines: the last one */
            ++o;
        }
        T.cur = ed_bol(o);
        err_message(msgbuf, err->code, err->line);
        show(msgbuf, C_ERROR);
    }
    lstart = o = ed_bol(T.cur);
    for (line = 1; o != 0; ++line) o = ed_bol((uint16_t)(o - 1));
    if (top == 0 || line < top || line > (uint16_t)(top + (TEXTROWS - 1))) {
        top = 1;                        /* show some lines above it */
        if (line > 8) top = (uint16_t)(line - 7);
    }
    toff = lstart;                      /* toff: back from the cursor */
    for (n = line; n != top; --n) toff = ed_bol((uint16_t)(toff - 1));
}

EdAction editor_run(TextBuf *t, const PL0Error *err_or_null)
{
    static uint8_t k, c;
    static uint16_t i;

    T = *t;
    if (!tbl_ready) {
        for (i = 0; i < 256; ++i) ed_tbl[i] = io_scr((uint8_t)i);
        tbl_ready = 1;
    }
    REG(0xD018) = 0x17;                 /* screen $0400, upper/lower case */
    REG(0xD020) = C_TEXT;
    REG(0xD021) = 6;                    /* blue */
    TEXTCOL = C_TEXT;
    memset(COLRAM, C_TEXT, 1000);
    BLNSW = 1;                          /* no KERNAL cursor */
    old_rpt = RPTFLG;
    old_mode = MODE;
    RPTFLG = 0x80;
    MODE = 0x80;
    NDX = 0;                            /* keys typed before the editor shows */
    place(err_or_null);
    curs = 0;
    dline = 0xFFFF;
    dirty = D_BAR;
    FULL();

    for (;;) {
        if (NDX == 0) {
            render();
            while (NDX == 0) ;
        }
        k = cbm_k_getin();
        if (k == 0) continue;
        if (msg) show(0, C_TEXT);       /* a message lasts until a key */
        if (k != K_UP && k != K_DOWN) want = 0;
        switch (k) {
        case K_F3:
            if (!discard_ok()) break;
            /* fall through */
        case K_F1: case K_F5:
            RPTFLG = old_rpt;
            MODE = old_mode;
            *t = T;
            if (k == K_F1) return ED_RUN;
            if (k == K_F3) return ED_OPEN;
            return ED_SAVE;
        case K_F7:   help_screen(); break;
        case K_F2:   if (discard_ok()) clear_text(); break;
        case K_RETURN: case K_SRET: k_insert('\n'); break;
        case K_DEL:  k_backspace(); break;
        case K_INST: k_delete(); break;
        case K_LEFT: k_left(); break;
        case K_RIGHT: k_right(); break;
        case K_UP:   k_up(); break;
        case K_DOWN: k_down(); break;
        case K_HOME: T.cur = lstart; break;
        case K_CLR:  T.cur = ed_eol(T.cur); break;
        default:
            c = key_ascii(k);
            if (c) k_insert(c);
        }
        follow_v();                     /* edits stay on visible lines */
    }
}

#pragma bss-name (pop)
