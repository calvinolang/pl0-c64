/* editor.c -- SPIKE weight stub: a gap-buffer line editor of realistic
 * size (target ~1.5 KB of 6502 code). Not reused. */
#include <string.h>
#include <conio.h>
#include "weight.h"

#define SCREEN ((unsigned char *)0x0400)
#define ROWS 23
#define COLS 40

static uint16_t gap_lo, gap_hi;      /* gap is [gap_lo, gap_hi) in source[] */
static uint16_t top_line, cur_line, cur_col;
static uint8_t dirty;

static uint16_t text_len(void) { return MAX_SOURCE - (gap_hi - gap_lo); }
static char at(uint16_t i) { return i < gap_lo ? source[i] : source[i + (gap_hi - gap_lo)]; }

static void move_gap(uint16_t pos)
{
    while (gap_lo > pos) source[--gap_hi] = source[--gap_lo];
    while (gap_lo < pos) source[gap_lo++] = source[gap_hi++];
}

static int ins(char c)
{
    if (gap_lo == gap_hi) return -1;
    source[gap_lo++] = c;
    dirty = 1;
    return 0;
}

static void del_left(void) { if (gap_lo) { --gap_lo; dirty = 1; } }
static void del_right(void) { if (gap_hi < MAX_SOURCE) { ++gap_hi; dirty = 1; } }

static uint16_t line_start(uint16_t pos)
{
    while (pos && at(pos - 1) != '\n') --pos;
    return pos;
}

static uint16_t line_end(uint16_t pos)
{
    uint16_t n = text_len();
    while (pos < n && at(pos) != '\n') ++pos;
    return pos;
}

static void recount(void)
{
    uint16_t i;
    cur_line = 0;
    for (i = 0; i < gap_lo; ++i) if (source[i] == '\n') ++cur_line;
    cur_col = gap_lo - line_start(gap_lo);
}

static void go_vert(int dir)
{
    uint16_t s = line_start(gap_lo), col = gap_lo - s, e;
    if (dir < 0) {
        if (!s) return;
        s = line_start(s - 1);
    } else {
        e = line_end(gap_lo);
        if (e >= text_len()) return;
        s = e + 1;
    }
    e = line_end(s);
    if (s + col > e) col = e - s;
    move_gap(s + col);
}

static unsigned char to_screen(char c)
{
    if (c >= 'a' && c <= 'z') return c - 'a' + 1;
    if (c >= 'A' && c <= 'Z') return c - 'A' + 65;
    if (c >= '@' && c <= '_') return c - '@';
    return c;
}

static void redraw(void)
{
    uint16_t pos = 0, n = text_len(), line = 0;
    unsigned char row, col, *p;
    if (cur_line < top_line) top_line = cur_line;
    if (cur_line >= top_line + ROWS) top_line = cur_line - ROWS + 1;
    while (line < top_line && pos < n) if (at(pos++) == '\n') ++line;
    for (row = 0; row < ROWS; ++row) {
        p = SCREEN + (row + 1) * COLS;
        for (col = 0; col < COLS; ++col) p[col] = ' ';
        for (col = 0; pos < n; ++pos) {
            char c = at(pos);
            if (c == '\n') { ++pos; break; }
            if (col < COLS) p[col++] = to_screen(c);
        }
    }
    gotoxy(0, 0);
    cprintf("L %u  C %u   ", cur_line + 1, cur_col + 1);
    gotoxy(cur_col < COLS ? cur_col : COLS - 1, cur_line - top_line + 1);
}

uint8_t editor_run(void)
{
    char k;
    gap_lo = 0; gap_hi = MAX_SOURCE;
    for (;;) {
        recount();
        redraw();
        k = cgetc();
        switch (k) {
        case CH_F1: return 1;
        case CH_F3: return 3;
        case CH_F5: return 5;
        case CH_F7: return 7;
        case CH_CURS_LEFT:  if (gap_lo) move_gap(gap_lo - 1); break;
        case CH_CURS_RIGHT: if (gap_lo < text_len()) move_gap(gap_lo + 1); break;
        case CH_CURS_UP:    go_vert(-1); break;
        case CH_CURS_DOWN:  go_vert(1); break;
        case CH_HOME:       move_gap(line_start(gap_lo)); break;
        case CH_DEL:        del_left(); break;
        case CH_INS:        del_right(); break;
        case CH_ENTER:      if (ins('\n')) return 0; break;
        default:
            if (k >= 32 && k < 127 && ins(k)) return 0;
            break;
        }
    }
}
