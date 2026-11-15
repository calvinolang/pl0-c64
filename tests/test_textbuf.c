#include "test.h"
#include "c64/textbuf.h"
#include <string.h>
#include <stdlib.h>

static char store[128];
static TextBuf tb;

static void puts_tb(const char *s) {
    for (; *s; s++) tb_insert(&tb, *s);
}
static void fresh(void) { tb_init(&tb, store, sizeof store); }

static void insert_and_text(void) {
    fresh();
    CHECK_EQ_STR(tb_text(&tb), "");
    puts_tb("abc");
    CHECK_EQ_STR(tb_text(&tb), "abc");
    CHECK_EQ_INT(tb_col(&tb), 4);
    CHECK_EQ_INT(tb_cursor(&tb), 3);
}
static void newline_counts_lines(void) {
    fresh();
    CHECK_EQ_INT(tb_line(&tb), 1);
    puts_tb("a\nbc\n");
    CHECK_EQ_INT(tb_line(&tb), 3);
    CHECK_EQ_INT(tb_col(&tb), 1);
}
static void backspace_at_start_noop(void) {
    fresh();
    tb_backspace(&tb);
    CHECK_EQ_STR(tb_text(&tb), "");
    puts_tb("xy");
    tb_home(&tb);
    tb_set_cursor(&tb, 0);
    tb_backspace(&tb);
    CHECK_EQ_STR(tb_text(&tb), "xy");
    tb_set_cursor(&tb, 2);
    tb_backspace(&tb);
    CHECK_EQ_STR(tb_text(&tb), "x");
}
static void up_down_keep_column(void) {
    fresh();
    puts_tb("abcdef\nab\nabcd");
    CHECK_EQ_INT(tb_col(&tb), 5);
    tb_up(&tb);
    CHECK_EQ_INT(tb_line(&tb), 2);
    CHECK_EQ_INT(tb_col(&tb), 3);   /* clamped to end of "ab" */
    tb_up(&tb);
    CHECK_EQ_INT(tb_line(&tb), 1);
    CHECK_EQ_INT(tb_col(&tb), 5);   /* sticky column returns */
    tb_up(&tb);                     /* no-op on line 1 */
    CHECK_EQ_INT(tb_line(&tb), 1);
    tb_down(&tb); tb_down(&tb);
    CHECK_EQ_INT(tb_line(&tb), 3);
    CHECK_EQ_INT(tb_col(&tb), 5);
    tb_down(&tb);                   /* no-op on last line */
    CHECK_EQ_INT(tb_line(&tb), 3);
}
static void sticky_column_survives_short_line(void) {
    fresh();
    puts_tb("abcdefgh\n\nabcdefgh");
    tb_set_cursor(&tb, 6);          /* line 1, col 7 */
    tb_down(&tb);
    CHECK_EQ_INT(tb_line(&tb), 2);
    CHECK_EQ_INT(tb_col(&tb), 1);
    tb_down(&tb);
    CHECK_EQ_INT(tb_col(&tb), 7);
    tb_left(&tb);                   /* any other motion resets stickiness */
    tb_up(&tb);
    CHECK_EQ_INT(tb_line(&tb), 2);
    CHECK_EQ_INT(tb_col(&tb), 1);
    tb_up(&tb);
    CHECK_EQ_INT(tb_col(&tb), 6);
}
static void home_end(void) {
    fresh();
    puts_tb("abc\ndef\nghi");
    tb_up(&tb);
    tb_home(&tb);
    CHECK_EQ_INT(tb_col(&tb), 1);
    CHECK_EQ_INT(tb_line(&tb), 2);
    tb_end(&tb);
    CHECK_EQ_INT(tb_col(&tb), 4);
    CHECK_EQ_INT(tb_line(&tb), 2);
    tb_down(&tb); tb_end(&tb);
    CHECK_EQ_INT(tb_col(&tb), 4);
    CHECK_EQ_INT(tb_line(&tb), 3);
}
static void left_right_cross_lines(void) {
    fresh();
    puts_tb("a\nb");
    tb_set_cursor(&tb, 2);
    tb_left(&tb);
    CHECK_EQ_INT(tb_line(&tb), 1);
    CHECK_EQ_INT(tb_col(&tb), 2);
    tb_right(&tb);
    CHECK_EQ_INT(tb_line(&tb), 2);
    CHECK_EQ_INT(tb_col(&tb), 1);
    tb_set_cursor(&tb, 0);
    tb_left(&tb);
    CHECK_EQ_INT(tb_cursor(&tb), 0);
    tb_set_cursor(&tb, 99);
    CHECK_EQ_INT(tb_cursor(&tb), 3);
    tb_right(&tb);
    CHECK_EQ_INT(tb_cursor(&tb), 3);
}
static void delete_joins_lines(void) {
    fresh();
    puts_tb("ab\ncd");
    tb_set_cursor(&tb, 2);
    tb_delete(&tb);
    CHECK_EQ_STR(tb_text(&tb), "abcd");
    CHECK_EQ_INT(tb_line_count(&tb), 1);
    tb_set_cursor(&tb, 4);
    tb_delete(&tb);
    CHECK_EQ_STR(tb_text(&tb), "abcd");
    tb_set_cursor(&tb, 2);
    tb_insert(&tb, '\n');
    tb_backspace(&tb);
    CHECK_EQ_STR(tb_text(&tb), "abcd");
}
static void goto_line(void) {
    fresh();
    puts_tb("one\ntwo\nthree");
    tb_goto_line(&tb, 2);
    CHECK_EQ_INT(tb_line(&tb), 2);
    CHECK_EQ_INT(tb_col(&tb), 1);
    tb_goto_line(&tb, 99);
    CHECK_EQ_INT(tb_line(&tb), 3);
    CHECK_EQ_INT(tb_col(&tb), 1);
    tb_goto_line(&tb, 1);
    CHECK_EQ_INT(tb_cursor(&tb), 0);
}
static void insert_in_middle_after_gap_moves(void) {
    fresh();
    puts_tb("hello world");
    tb_set_cursor(&tb, 5);
    puts_tb(",");
    tb_set_cursor(&tb, 0);
    puts_tb(">");
    tb_set_cursor(&tb, 99);
    puts_tb("!");
    CHECK_EQ_STR(tb_text(&tb), ">hello, world!");
}
static void tb_at_matches_text_for_every_offset(void) {
    unsigned i, c;
    char exp[64];
    fresh();
    puts_tb("ab\ncdef\ng");
    strcpy(exp, "ab\ncdef\ng");
    for (c = 0; c <= 9; c++) {
        tb_set_cursor(&tb, (uint16_t)c);   /* gap at every position */
        CHECK_EQ_INT(tb_len(&tb), 9);
        for (i = 0; i < 9; i++) CHECK_EQ_INT(tb_at(&tb, (uint16_t)i), exp[i]);
    }
    tb_set_cursor(&tb, 4);
    CHECK_EQ_STR(tb_text(&tb), exp);
}
static void offset_of_line_and_line_count(void) {
    fresh();
    CHECK_EQ_INT(tb_line_count(&tb), 1);
    CHECK_EQ_INT(tb_offset_of_line(&tb, 1), 0);
    CHECK_EQ_INT(tb_offset_of_line(&tb, 2), 0);   /* beyond end = len */
    puts_tb("ab\ncd\n");
    CHECK_EQ_INT(tb_line_count(&tb), 3);          /* trailing '\n' */
    CHECK_EQ_INT(tb_offset_of_line(&tb, 1), 0);
    CHECK_EQ_INT(tb_offset_of_line(&tb, 2), 3);
    CHECK_EQ_INT(tb_offset_of_line(&tb, 3), 6);
    CHECK_EQ_INT(tb_offset_of_line(&tb, 4), 6);
    CHECK_EQ_INT(tb_offset_of_line(&tb, 500), 6);
    tb_set_cursor(&tb, 1);                        /* gap in the middle */
    CHECK_EQ_INT(tb_offset_of_line(&tb, 2), 3);
    CHECK_EQ_INT(tb_line_count(&tb), 3);
}
static void buffer_full_returns_minus_one_and_text_intact(void) {
    char small[5];
    TextBuf s;
    tb_init(&s, small, sizeof small);
    CHECK_EQ_INT(tb_insert(&s, 'a'), 0);
    CHECK_EQ_INT(tb_insert(&s, 'b'), 0);
    CHECK_EQ_INT(tb_insert(&s, 'c'), 0);
    CHECK_EQ_INT(tb_insert(&s, 'd'), 0);
    CHECK_EQ_INT(tb_insert(&s, 'e'), -1);
    CHECK_EQ_STR(tb_text(&s), "abcd");
    tb_set_cursor(&s, 2);
    CHECK_EQ_INT(tb_insert(&s, 'x'), -1);
    CHECK_EQ_STR(tb_text(&s), "abcd");
    CHECK_EQ_INT(tb_cursor(&s), 2);
}
static void rejects_control_characters(void) {
    fresh();
    puts_tb("a");
    CHECK_EQ_INT(tb_insert(&tb, '\t'), -2);
    CHECK_EQ_INT(tb_insert(&tb, '\r'), -2);
    CHECK_EQ_INT(tb_insert(&tb, 0), -2);
    CHECK_EQ_INT(tb_insert(&tb, (char)0x7f), -2);
    CHECK_EQ_INT(tb_insert(&tb, (char)0xC1), -2);
    CHECK_EQ_INT(tb_insert(&tb, (char)0x1f), -2);
    CHECK_EQ_INT(tb_insert(&tb, ' '), 0);
    CHECK_EQ_INT(tb_insert(&tb, '~'), 0);
    CHECK_EQ_INT(tb_insert(&tb, '\n'), 0);
    CHECK_EQ_STR(tb_text(&tb), "a ~\n");
}
static void set_replaces(void) {
    fresh();
    puts_tb("old text");
    CHECK_EQ_INT(tb_set(&tb, "new\nstuff"), 0);
    CHECK_EQ_STR(tb_text(&tb), "new\nstuff");
    CHECK_EQ_INT(tb_cursor(&tb), 0);
    CHECK_EQ_INT(tb_line_count(&tb), 2);
    puts_tb(">");
    CHECK_EQ_STR(tb_text(&tb), ">new\nstuff");
    CHECK_EQ_INT(tb_set(&tb, ""), 0);
    CHECK_EQ_STR(tb_text(&tb), "");
}
static void set_too_big_leaves_unchanged(void) {
    char small[4];
    TextBuf s;
    tb_init(&s, small, sizeof small);
    tb_insert(&s, 'a'); tb_insert(&s, 'b');
    CHECK(tb_set(&s, "abcd") != 0);        /* 4 chars needs cap 5 */
    CHECK_EQ_STR(tb_text(&s), "ab");
    CHECK_EQ_INT(tb_cursor(&s), 2);
    CHECK_EQ_INT(tb_set(&s, "abc"), 0);
    CHECK_EQ_STR(tb_text(&s), "abc");
}

/* ---- differential test against a plain-array reference model ---- */
#define RCAP 64
typedef struct { char a[RCAP]; unsigned len, cur, want; } Ref;

static unsigned r_ls(const Ref *r) {
    unsigned p = r->cur;
    while (p > 0 && r->a[p - 1] != '\n') p--;
    return p;
}
static unsigned r_le(const Ref *r, unsigned p) {
    while (p < r->len && r->a[p] != '\n') p++;
    return p;
}
static unsigned r_line(const Ref *r) {
    unsigned i, n = 1;
    for (i = 0; i < r->cur; i++) if (r->a[i] == '\n') n++;
    return n;
}
static unsigned r_col(const Ref *r) { return r->cur - r_ls(r) + 1; }
static void r_ins(Ref *r, char c) {
    if (r->len + 1 >= RCAP) return;   /* failed insert keeps the sticky column */
    r->want = 0;
    memmove(r->a + r->cur + 1, r->a + r->cur, r->len - r->cur);
    r->a[r->cur++] = c;
    r->len++;
}
static void r_bs(Ref *r) {
    r->want = 0;
    if (r->cur == 0) return;
    memmove(r->a + r->cur - 1, r->a + r->cur, r->len - r->cur);
    r->cur--; r->len--;
}
static void r_del(Ref *r) {
    r->want = 0;
    if (r->cur == r->len) return;
    memmove(r->a + r->cur, r->a + r->cur + 1, r->len - r->cur - 1);
    r->len--;
}
static void r_up(Ref *r) {
    unsigned ls, ps, plen, k;
    if (r->want == 0) r->want = r_col(r);
    ls = r_ls(r);
    if (ls == 0) return;
    ps = ls - 1;
    while (ps > 0 && r->a[ps - 1] != '\n') ps--;
    plen = ls - 1 - ps;
    k = r->want - 1;
    if (k > plen) k = plen;
    r->cur = ps + k;
}
static void r_down(Ref *r) {
    unsigned e, s, len, k;
    if (r->want == 0) r->want = r_col(r);
    e = r_le(r, r->cur);
    if (e == r->len) return;
    s = e + 1;
    len = r_le(r, s) - s;
    k = r->want - 1;
    if (k > len) k = len;
    r->cur = s + k;
}
static void r_goto(Ref *r, unsigned line) {
    unsigned p = 0, n = 1;
    r->want = 0;
    if (line < 1) line = 1;
    while (n < line) {
        unsigned e = r_le(r, p);
        if (e == r->len) break;     /* last line: stay at its start */
        p = e + 1; n++;
    }
    r->cur = p;
}
static int same(TextBuf *t, const Ref *r) {
    return tb_len(t) == r->len && tb_cursor(t) == r->cur &&
           tb_line(t) == r_line(r) && tb_col(t) == r_col(r) &&
           memcmp(tb_text(t), r->a, r->len) == 0 && tb_text(t)[r->len] == 0;
}
static const char alpha[] = "abc XY\n\n\n";

static void differential_random(void) {
    static char st[RCAP];
    TextBuf t;
    Ref r;
    unsigned long seed = 12345UL;
    unsigned n, bad = 0;

    tb_init(&t, st, RCAP);
    memset(&r, 0, sizeof r);
    for (n = 0; n < 20000 && !bad; n++) {
        unsigned op, v;
        seed = seed * 1103515245UL + 12345UL;
        seed &= 0x7fffffffUL;
        op = (unsigned)(seed >> 16) % 17;
        seed = seed * 1103515245UL + 12345UL;
        seed &= 0x7fffffffUL;
        v = (unsigned)(seed >> 16);
        switch (op) {
        case 0: case 1: case 2: case 3: case 4: {
            char c = alpha[v % (sizeof alpha - 1)];
            int rc = tb_insert(&t, c);
            CHECK_EQ_INT(rc, (r.len + 1 >= RCAP) ? -1 : 0);
            r_ins(&r, c);
            break; }
        case 5: tb_backspace(&t); r_bs(&r); break;
        case 6: tb_delete(&t); r_del(&r); break;
        case 7: tb_left(&t); r.want = 0; if (r.cur) r.cur--; break;
        case 8: tb_right(&t); r.want = 0; if (r.cur < r.len) r.cur++; break;
        case 9: case 10: tb_up(&t); r_up(&r); break;
        case 11: case 12: tb_down(&t); r_down(&r); break;
        case 13: tb_home(&t); r.want = 0; r.cur = r_ls(&r); break;
        case 14: tb_end(&t); r.want = 0; r.cur = r_le(&r, r.cur); break;
        case 15: v %= 12; tb_goto_line(&t, (uint16_t)v); r_goto(&r, v); break;
        default:
            v %= (r.len + 2);
            tb_set_cursor(&t, (uint16_t)v);
            r.want = 0;
            r.cur = v > r.len ? r.len : v;
            break;
        }
        if (!same(&t, &r)) {
            bad = 1;
            printf("differential mismatch at op #%u (op %u) tb len/cur/want %u/%u/%u ref %u/%u/%u\n", n, op, tb_len(&t), tb_cursor(&t), t.want, r.len, r.cur, r.want);
        }
        if (tb_len(&t) > 0) {
            unsigned i = v % tb_len(&t);
            if (tb_at(&t, (uint16_t)i) != r.a[i]) bad = 1;
        }
    }
    CHECK(!bad);
    CHECK_EQ_INT(n, 20000);
}

int main(void) {
    insert_and_text();
    newline_counts_lines();
    backspace_at_start_noop();
    up_down_keep_column();
    sticky_column_survives_short_line();
    home_end();
    left_right_cross_lines();
    delete_joins_lines();
    goto_line();
    insert_in_middle_after_gap_moves();
    tb_at_matches_text_for_every_offset();
    offset_of_line_and_line_count();
    buffer_full_returns_minus_one_and_text_intact();
    rejects_control_characters();
    set_replaces();
    set_too_big_leaves_unchanged();
    differential_random();
    TEST_MAIN_END();
}
