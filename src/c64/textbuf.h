#ifndef TEXTBUF_H
#define TEXTBUF_H
/* Editor text buffer: a gap buffer over caller-provided storage.
 * Portable C (host tests and cc65). No heap, no recursion.
 *
 * Layout: buf[0..gs) is the text before the gap, buf[gs..gs+gap) is the
 * gap (gap = cap - len), the rest is the remaining text. The gap moves only
 * when editing, so cursor motion costs nothing and tb_text keeps the cursor.
 * Capacity counts a terminating NUL, so the usable length is cap-1.
 * Lines and columns are 1-based. Text holds only 0x20..0x7E and '\n'. */
#include <stdint.h>

/* The C64 program does its line scans in assembly (edscreen.s), so the
 * cc65 build leaves out the textbuf line queries and vertical moves (about
 * 950 bytes; ld65 links whole objects). Define TB_LINE_SCANS to keep them.
 * The host build and its tests always have them. */
#if defined(__CC65__) && !defined(TB_LINE_SCANS)
#define TB_NO_LINE_SCANS 1
#endif

typedef struct {
    char *buf;
    uint16_t cap;   /* size of storage */
    uint16_t len;   /* characters stored (excluding the gap) */
    uint16_t cur;   /* cursor: logical offset (independent of the gap) */
    uint16_t gs;    /* gap start (logical offset); moved lazily by edits */
    uint16_t want;  /* sticky column for up/down; 0 = none */
} TextBuf;

/* cap must be >= 1. Empty buffer, cursor at 0. */
void tb_init(TextBuf *t, char *storage, uint16_t cap);
/* 0 ok, -1 full (text unchanged), -2 character not accepted (unchanged;
 * only 0x20..0x7E and '\n' are accepted; -2 is reported before -1).
 * Inserts before the cursor. A failed insert changes nothing, including the
 * sticky column; any successful edit or non-vertical motion resets it. */
int  tb_insert(TextBuf *t, char c);
void tb_backspace(TextBuf *t);  /* no-op at start of buffer */
void tb_delete(TextBuf *t);     /* no-op at end of buffer */
void tb_left(TextBuf *t);       /* cross line boundaries */
void tb_right(TextBuf *t);
#ifndef TB_NO_LINE_SCANS
void tb_up(TextBuf *t);         /* sticky desired column; no-op on line 1 */
void tb_down(TextBuf *t);       /* no-op on the last line */
void tb_home(TextBuf *t);       /* start of the current line */
void tb_end(TextBuf *t);        /* end of the current line (before '\n') */
/* Cost: tb_line, tb_col, tb_line_count, tb_offset_of_line (and up/down/
 * home/end/goto_line) scan the text, O(buffer) / O(line); the editor should
 * cache the current line number rather than call tb_line per redraw. */
uint16_t tb_line(const TextBuf *t);
uint16_t tb_col(const TextBuf *t);
/* Line 0 means line 1; past the end clamps to the start of the last line. */
void tb_goto_line(TextBuf *t, uint16_t line);
#endif
/* NUL-terminated whole text; moves the gap to the end. The pointer stays
 * valid until the next edit. */
const char *tb_text(TextBuf *t);
/* Replace contents; cursor to 0. Returns nonzero (buffer unchanged, cursor
 * and sticky column too) if the string does not fit in cap-1 characters.
 * s must NOT point into the buffer's own storage: tb_set(&t, tb_text(&t))
 * corrupts the text (no guard: it cost 63 bytes on the C64).
 * Characters are not filtered. */
int  tb_set(TextBuf *t, const char *s);

uint16_t tb_len(const TextBuf *t);                 /* characters */
char     tb_at(const TextBuf *t, uint16_t i);      /* i < len, gap-aware */
#ifndef TB_NO_LINE_SCANS
/* Offset of the first character of 1-based line; tb_len if beyond the end.
 * A trailing '\n' starts a new, empty line. */
uint16_t tb_offset_of_line(const TextBuf *t, uint16_t line);
uint16_t tb_line_count(const TextBuf *t);          /* 1 for an empty buffer */
#endif
uint16_t tb_cursor(const TextBuf *t);
void     tb_set_cursor(TextBuf *t, uint16_t off);  /* clamps to len */

#endif
