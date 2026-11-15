#include "c64/textbuf.h"
#include <string.h>

/* Gap buffer. Portable C; no heap, no recursion, no multiplication/division.
 * cc65 notes: declarations at block start; no `x = c == K ? K2 : c;` and no
 * (int16_t)uchar - uchar (both miscompile under -O, see spike-results.md);
 * all counts are uint16_t. Scans walk the two segments with plain pointers
 * (much smaller and faster than per-character index arithmetic on cc65). */

#define GAP(t) ((uint16_t)((t)->cap - (t)->len))

void tb_init(TextBuf *t, char *storage, uint16_t cap) {
    t->buf = storage;
    t->cap = cap;
    t->len = t->cur = t->gs = t->want = 0;
}

uint16_t tb_len(const TextBuf *t) { return t->len; }
uint16_t tb_cursor(const TextBuf *t) { return t->cur; }

char tb_at(const TextBuf *t, uint16_t i) {
    const char *p = t->buf + i;
    if (i >= t->gs) p += GAP(t);
    return *p;
}

/* Move the gap to logical offset off (<= len). The cursor is independent of
 * the gap: motion only changes cur; the gap moves lazily on edits.
 * memmove: the two ranges can overlap when the gap is small. */
static void move_gap(TextBuf *t, uint16_t off) {
    uint16_t gs = t->gs;
    char *src = t->buf + gs;       /* start of the text after the gap */
    char *dst = src + (uint16_t)(t->cap - t->len);
    if (off < gs) {
        /* move [off, gs) up to the end of the gap */
        memmove(dst - (uint16_t)(gs - off), src - (uint16_t)(gs - off),
                (uint16_t)(gs - off));
    } else {
        memmove(src, dst, (uint16_t)(off - gs));
    }
    t->gs = off;
}

void tb_set_cursor(TextBuf *t, uint16_t off) {
    if (off > t->len) off = t->len;
    t->cur = off;
    t->want = 0;
}

int tb_insert(TextBuf *t, char c) {
    unsigned char u = (unsigned char)c;
    if (u != 10 && (u < 0x20 || u > 0x7E)) return -2;
    if ((uint16_t)(t->len + 1) >= t->cap) return -1;
    move_gap(t, t->cur);
    t->buf[t->gs++] = c;
    t->cur++;
    t->len++;
    t->want = 0;
    return 0;
}

void tb_delete(TextBuf *t) {
    if (t->cur != t->len) {
        move_gap(t, t->cur);
        t->len--;               /* gap grows over the next character */
    }
    t->want = 0;
}

void tb_backspace(TextBuf *t) {
    if (t->cur != 0) {
        t->cur--;
        tb_delete(t);           /* deletes the character before the old cursor */
    }
    t->want = 0;
}

void tb_left(TextBuf *t) {
    if (t->cur != 0) t->cur--;
    t->want = 0;
}

void tb_right(TextBuf *t) {
    if (t->cur != t->len) t->cur++;
    t->want = 0;
}

/* Scans use tb_at per character: on cc65 that is smaller than hand-walked
 * segment pointers (measured; see the report). */

/* Offset of the start of the line containing offset p. */
static uint16_t line_start(const TextBuf *t, uint16_t p) {
    while (p != 0 && tb_at(t, (uint16_t)(p - 1)) != '\n') p--;
    return p;
}

/* Offset of the '\n' ending the line containing p, or len. */
static uint16_t line_end(const TextBuf *t, uint16_t p) {
    while (p < t->len && tb_at(t, p) != '\n') p++;
    return p;
}

/* 1 + number of '\n' in logical [0, n). */
static uint16_t line_of(const TextBuf *t, uint16_t n) {
    uint16_t i, c = 1;
    for (i = 0; i < n; i++)
        if (tb_at(t, i) == '\n') c++;
    return c;
}

uint16_t tb_line(const TextBuf *t) { return line_of(t, t->cur); }
uint16_t tb_line_count(const TextBuf *t) { return line_of(t, t->len); }

/* Start of 1-based line (0 counts as 1). Past the last line: its start if
 * clamp, else len. */
static uint16_t nth_line(const TextBuf *t, uint16_t line, uint8_t clamp) {
    uint16_t p = 0, e;
    while (line > 1) {
        e = line_end(t, p);
        if (e >= t->len) return clamp ? p : t->len;
        p = (uint16_t)(e + 1);
        line--;
    }
    return p;
}

uint16_t tb_offset_of_line(const TextBuf *t, uint16_t line) {
    return nth_line(t, line, 0);
}
void tb_goto_line(TextBuf *t, uint16_t line) {
    t->cur = nth_line(t, line, 1);
    t->want = 0;
}
uint16_t tb_col(const TextBuf *t) {
    return (uint16_t)(t->cur - line_start(t, t->cur) + 1);
}

void tb_home(TextBuf *t) {
    t->cur = line_start(t, t->cur);
    t->want = 0;
}
void tb_end(TextBuf *t) {
    t->cur = line_end(t, t->cur);
    t->want = 0;
}

/* Up/down share one routine: find the target line [s, en], then put the
 * cursor at column want (clamped to the line's length). */
static void vmove(TextBuf *t, uint8_t down) {
    uint16_t s, en, k;
    if (t->want == 0) t->want = tb_col(t);
    if (down) {
        en = line_end(t, t->cur);
        if (en == t->len) return;
        s = (uint16_t)(en + 1);
        en = line_end(t, s);
    } else {
        s = line_start(t, t->cur);
        if (s == 0) return;
        en = (uint16_t)(s - 1);
        s = line_start(t, en);
    }
    k = (uint16_t)(t->want - 1);
    if (k > (uint16_t)(en - s)) k = (uint16_t)(en - s);
    t->cur = (uint16_t)(s + k);
}
void tb_up(TextBuf *t) { vmove(t, 0); }
void tb_down(TextBuf *t) { vmove(t, 1); }

const char *tb_text(TextBuf *t) {
    move_gap(t, t->len);
    t->buf[t->len] = 0;     /* len < cap is guaranteed by tb_insert/tb_set */
    return t->buf;
}

int tb_set(TextBuf *t, const char *s) {
    uint16_t n = 0;
    char *d;
    while (s[n] != 0) n++;
    if (n >= t->cap) return 1;
    d = t->buf + (uint16_t)(t->cap - n);
    memcpy(d, s, n);
    t->len = n;
    t->cur = t->gs = t->want = 0;   /* gap at the start, text after it */
    return 0;
}
