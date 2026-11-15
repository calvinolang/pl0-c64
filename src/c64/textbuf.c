#include "c64/textbuf.h"

/* Gap buffer. Portable C; no heap, no recursion, no multiplication/division.
 * cc65 notes: declarations at block start; no `x = c == K ? K2 : c;` and no
 * (int16_t)uchar - uchar (both miscompile under -O, see spike-results.md);
 * all counts are uint16_t. */

#define GAP(t) ((uint16_t)((t)->cap - (t)->len))

void tb_init(TextBuf *t, char *storage, uint16_t cap) {
    t->buf = storage;
    t->cap = cap;
    t->len = 0;
    t->cur = 0;
    t->gs = 0;
    t->want = 0;
}

uint16_t tb_len(const TextBuf *t) { return t->len; }
uint16_t tb_cursor(const TextBuf *t) { return t->cur; }

char tb_at(const TextBuf *t, uint16_t i) {
    if (i < t->gs) return t->buf[i];
    return t->buf[(uint16_t)(i + GAP(t))];
}

/* Move the gap to logical offset off (<= len). The cursor is independent of
 * the gap: motion only changes cur; the gap moves lazily on edits. */
static void move_gap(TextBuf *t, uint16_t off) {
    uint16_t gap = GAP(t);
    uint16_t k;
    char *b = t->buf;
    if (off < t->gs) {
        for (k = t->gs; k > off; k--)
            b[(uint16_t)(k - 1 + gap)] = b[(uint16_t)(k - 1)];
    } else {
        for (k = t->gs; k < off; k++)
            b[k] = b[(uint16_t)(k + gap)];
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
    if (!(u == 10 || (u >= 0x20 && u <= 0x7E))) return -2;
    if ((uint16_t)(t->len + 1) >= t->cap) return -1;
    move_gap(t, t->cur);
    t->buf[t->gs] = c;
    t->gs++;
    t->cur++;
    t->len++;
    t->want = 0;
    return 0;
}

void tb_backspace(TextBuf *t) {
    t->want = 0;
    if (t->cur == 0) return;
    move_gap(t, t->cur);
    t->gs--;
    t->cur--;
    t->len--;
}

void tb_delete(TextBuf *t) {
    t->want = 0;
    if (t->cur == t->len) return;
    move_gap(t, t->cur);
    t->len--;               /* gap grows over the next character */
}

void tb_left(TextBuf *t) {
    t->want = 0;
    if (t->cur > 0) t->cur--;
}

void tb_right(TextBuf *t) {
    t->want = 0;
    if (t->cur < t->len) t->cur++;
}

/* Offset of the start of the line containing offset p. */
static uint16_t line_start(const TextBuf *t, uint16_t p) {
    while (p > 0 && tb_at(t, (uint16_t)(p - 1)) != '\n') p--;
    return p;
}

/* Offset of the '\n' ending the line containing p, or len. */
static uint16_t line_end(const TextBuf *t, uint16_t p) {
    while (p < t->len && tb_at(t, p) != '\n') p++;
    return p;
}

void tb_home(TextBuf *t) {
    t->cur = line_start(t, t->cur);
    t->want = 0;
}

void tb_end(TextBuf *t) {
    t->cur = line_end(t, t->cur);
    t->want = 0;
}

uint16_t tb_line(const TextBuf *t) {
    uint16_t i, n = 1;
    for (i = 0; i < t->cur; i++)
        if (tb_at(t, i) == '\n') n++;
    return n;
}

uint16_t tb_col(const TextBuf *t) {
    return (uint16_t)(t->cur - line_start(t, t->cur) + 1);
}

void tb_up(TextBuf *t) {
    uint16_t ls, ps, plen, k;
    if (t->want == 0) t->want = tb_col(t);
    ls = line_start(t, t->cur);
    if (ls == 0) return;
    ps = line_start(t, (uint16_t)(ls - 1));
    plen = (uint16_t)(ls - 1 - ps);
    k = (uint16_t)(t->want - 1);
    if (k > plen) k = plen;
    t->cur = (uint16_t)(ps + k);
}

void tb_down(TextBuf *t) {
    uint16_t e, s, len, k;
    if (t->want == 0) t->want = tb_col(t);
    e = line_end(t, t->cur);
    if (e == t->len) return;
    s = (uint16_t)(e + 1);
    len = (uint16_t)(line_end(t, s) - s);
    k = (uint16_t)(t->want - 1);
    if (k > len) k = len;
    t->cur = (uint16_t)(s + k);
}

uint16_t tb_offset_of_line(const TextBuf *t, uint16_t line) {
    uint16_t p = 0, n = 1;
    while (n < line) {
        p = line_end(t, p);
        if (p >= t->len) return t->len;
        p++;
        n++;
    }
    return p;
}

uint16_t tb_line_count(const TextBuf *t) {
    uint16_t i, n = 1;
    for (i = 0; i < t->len; i++)
        if (tb_at(t, i) == '\n') n++;
    return n;
}

void tb_goto_line(TextBuf *t, uint16_t line) {
    /* offset_of_line returns len for a line past the end; that would be the
     * end of the last line, so clamp to the last line's start instead. */
    uint16_t cnt = tb_line_count(t);
    if (line > cnt) line = cnt;
    t->cur = tb_offset_of_line(t, line);
    t->want = 0;
}

const char *tb_text(TextBuf *t) {
    move_gap(t, t->len);
    t->buf[t->len] = 0;     /* len < cap is guaranteed by tb_insert/tb_set */
    return t->buf;
}

int tb_set(TextBuf *t, const char *s) {
    uint16_t n = 0, i, gap;
    while (s[n] != 0) {
        n++;
        if (n >= t->cap) return 1;
    }
    gap = (uint16_t)(t->cap - n);
    for (i = 0; i < n; i++) t->buf[(uint16_t)(gap + i)] = s[i];
    t->len = n;
    t->cur = 0;
    t->gs = 0;      /* gap at the start, all text after it */
    t->want = 0;
    return 0;
}
