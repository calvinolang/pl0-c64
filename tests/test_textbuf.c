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

/* ---- differential test against a plain-array reference model ----
 * The model derives everything from first principles: it lists the start
 * offset of every line by scanning the array for '\n', and moves up/down/
 * goto by line number, not by mirroring the implementation's scans. */
#define RCAP 64
typedef struct { char a[RCAP]; unsigned len, cur, want; } Ref;

/* Fill st[] with line start offsets; returns the number of lines. */
static unsigned r_starts(const Ref *r, unsigned *st) {
    unsigned i, n = 1;
    st[0] = 0;
    for (i = 0; i < r->len; i++)
        if (r->a[i] == '\n') st[n++] = i + 1;
    return n;
}
static unsigned r_linelen(const Ref *r, const unsigned *st, unsigned n, unsigned l) {
    return (l + 1 < n ? st[l + 1] - 1 : r->len) - st[l];
}
/* 0-based line index of the cursor */
static unsigned r_lineidx(const Ref *r, const unsigned *st, unsigned n) {
    unsigned l = 0;
    while (l + 1 < n && st[l + 1] <= r->cur) l++;
    return l;
}
static void r_ins(Ref *r, char c) {
    r->want = 0;
    memmove(r->a + r->cur + 1, r->a + r->cur, r->len - r->cur);
    r->a[r->cur++] = c;
    r->len++;
}
static void r_del_at(Ref *r, unsigned pos) {
    memmove(r->a + pos, r->a + pos + 1, r->len - pos - 1);
    r->len--;
}
static void r_vert(Ref *r, int dir) {
    unsigned st[RCAP + 1], n = r_starts(r, st);
    unsigned l = r_lineidx(r, st, n), k;
    if (r->want == 0) r->want = r->cur - st[l] + 1;
    if (dir < 0 ? l == 0 : l + 1 == n) return;
    l = dir < 0 ? l - 1 : l + 1;
    k = r_linelen(r, st, n, l);
    if (r->want - 1 < k) k = r->want - 1;
    r->cur = st[l] + k;
}
static void r_goto(Ref *r, unsigned line) {
    unsigned st[RCAP + 1], n = r_starts(r, st);
    r->want = 0;
    if (line < 1) line = 1;
    if (line > n) line = n;
    r->cur = st[line - 1];
}

static const char alpha[] = "abc XY\n\n\n";
static const char bad_chars[] = { '\t', '\r', 0, 0x7f, 0x1f, (char)0x80, (char)0xFF };

/* Full comparison of the buffer against the model; uses tb_at only, so the
 * gap stays wherever the last edit left it. Returns nonzero on mismatch. */
static int differs(TextBuf *t, const Ref *r) {
    unsigned i, st[RCAP + 1], n = r_starts(r, st), l = r_lineidx(r, st, n);
    if (tb_len(t) != r->len || tb_cursor(t) != r->cur) return 1;
    if (t->want != r->want) return 2;
    if (tb_line(t) != l + 1 || tb_col(t) != r->cur - st[l] + 1) return 3;
    if (tb_line_count(t) != n) return 4;
    for (i = 0; i < r->len; i++)
        if (tb_at(t, (uint16_t)i) != r->a[i]) return 5;
    for (i = 1; i <= n + 2; i++)
        if (tb_offset_of_line(t, (uint16_t)i) != (i <= n ? st[i - 1] : r->len))
            return 6;
    return 0;
}

static void differential_random(void) {
    static char st_[RCAP];
    TextBuf t;
    Ref r;
    unsigned long seed = 12345UL;
    unsigned n, bad = 0, sets_ok = 0, sets_big = 0, invalid = 0, mid_gap = 0;

    tb_init(&t, st_, RCAP);
    memset(&r, 0, sizeof r);
    for (n = 0; n < 20000 && !bad; n++) {
        unsigned op, v, d;
        seed = seed * 1103515245UL + 12345UL; seed &= 0x7fffffffUL;
        op = (unsigned)(seed >> 16) % 21;
        seed = seed * 1103515245UL + 12345UL; seed &= 0x7fffffffUL;
        v = (unsigned)(seed >> 16);
        switch (op) {
        case 0: case 1: case 2: case 3: case 4: {
            char c = alpha[v % (sizeof alpha - 1)];
            int rc = tb_insert(&t, c);
            CHECK_EQ_INT(rc, (r.len + 1 >= RCAP) ? -1 : 0);
            if (rc == 0) r_ins(&r, c);
            break; }
        case 5:
            tb_backspace(&t);
            r.want = 0;
            if (r.cur) { r_del_at(&r, r.cur - 1); r.cur--; }
            break;
        case 6:
            tb_delete(&t);
            r.want = 0;
            if (r.cur < r.len) r_del_at(&r, r.cur);
            break;
        case 7: tb_left(&t); r.want = 0; if (r.cur) r.cur--; break;
        case 8: tb_right(&t); r.want = 0; if (r.cur < r.len) r.cur++; break;
        case 9: case 10: tb_up(&t); r_vert(&r, -1); break;
        case 11: case 12: tb_down(&t); r_vert(&r, 1); break;
        case 13: {
            unsigned st[RCAP + 1], k = r_starts(&r, st);
            tb_home(&t); r.want = 0; r.cur = st[r_lineidx(&r, st, k)];
            break; }
        case 14: {
            unsigned st[RCAP + 1], k = r_starts(&r, st), l = r_lineidx(&r, st, k);
            tb_end(&t); r.want = 0; r.cur = st[l] + r_linelen(&r, st, k, l);
            break; }
        case 15: /* includes 0 and lines beyond the end, and a huge line */
            d = v % 14;
            if (d == 13) d = 0xFFFF;
            tb_goto_line(&t, (uint16_t)d);
            r_goto(&r, d);
            break;
        case 16: /* includes offsets beyond the end */
            d = v % (r.len + 4);
            tb_set_cursor(&t, (uint16_t)d);
            r.want = 0;
            r.cur = d > r.len ? r.len : d;
            break;
        case 17: case 18: { /* tb_set with a random string, 0..71 chars */
            char s[80];
            unsigned i, len = v % 72;
            int rc;
            for (i = 0; i < len; i++) {
                seed = seed * 1103515245UL + 12345UL; seed &= 0x7fffffffUL;
                s[i] = alpha[(unsigned)(seed >> 16) % (sizeof alpha - 1)];
            }
            s[len] = 0;
            rc = tb_set(&t, s);
            if (len >= RCAP) {              /* too big: nothing changes */
                CHECK(rc != 0);
                sets_big++;
            } else {
                CHECK_EQ_INT(rc, 0);
                memcpy(r.a, s, len);
                r.len = len; r.cur = 0; r.want = 0;
                sets_ok++;
            }
            break; }
        case 19: { /* invalid character: nothing changes, sticky survives */
            char c = bad_chars[v % sizeof bad_chars];
            CHECK_EQ_INT(tb_insert(&t, c), -2);
            invalid++;
            break; }
        default: /* occasionally: tb_text must keep text and cursor */
            if (v % 8 == 0) {
                const char *p = tb_text(&t);
                CHECK(memcmp(p, r.a, r.len) == 0 && p[r.len] == 0);
            }
            break;
        }
        if (t.gs != 0 && t.gs != t.len) mid_gap++;
        d = (unsigned)differs(&t, &r);
        if (d) {
            bad = 1;
            printf("differential mismatch at op #%u (op %u), reason %u: tb len/cur/want %u/%u/%u ref %u/%u/%u\n",
                   n, op, d, tb_len(&t), tb_cursor(&t), t.want, r.len, r.cur, r.want);
        }
        if (n % 64 == 63) {                 /* occasionally the whole text */
            const char *p = tb_text(&t);
            if (memcmp(p, r.a, r.len) != 0 || p[r.len] != 0 ||
                tb_cursor(&t) != r.cur) bad = 1;
        }
    }
    { const char *p = tb_text(&t);
      CHECK(memcmp(p, r.a, r.len) == 0 && p[r.len] == 0); }
    CHECK(!bad);
    CHECK_EQ_INT(n, 20000);
    CHECK(sets_ok > 100 && sets_big > 100 && invalid > 100);
    CHECK(mid_gap > 5000);                  /* gap really was mid-text */
}

static void failed_insert_keeps_sticky_column(void) {
    fresh();
    puts_tb("abcdef\nab\nabcdef");
    tb_set_cursor(&tb, 5);
    tb_down(&tb);
    CHECK_EQ_INT(tb_col(&tb), 3);
    CHECK_EQ_INT(tb_insert(&tb, '\t'), -2);     /* -2 changes nothing */
    tb_down(&tb);
    CHECK_EQ_INT(tb_col(&tb), 6);               /* sticky column survived */
    tb_up(&tb);
    {   /* -1: full buffer */
        char small[4];
        TextBuf s;
        tb_init(&s, small, sizeof small);
        tb_set(&s, "a\nb");                     /* 3 chars, full */
        tb_set_cursor(&s, 3);
        tb_up(&s);                              /* want = 2 */
        CHECK_EQ_INT(tb_insert(&s, 'x'), -1);
        tb_down(&s);
        CHECK_EQ_INT(tb_col(&s), 2);
        CHECK_EQ_INT(tb_insert(&s, '\t'), -2);  /* -2 reported before -1 */
        CHECK_EQ_INT(tb_insert(&s, 'x'), -1);
    }
}
static void goto_line_zero_is_line_one(void) {
    fresh();
    puts_tb("a\nb\nc");
    tb_goto_line(&tb, 0);
    CHECK_EQ_INT(tb_cursor(&tb), 0);
    tb_goto_line(&tb, 0xFFFF);
    CHECK_EQ_INT(tb_line(&tb), 3);
    CHECK_EQ_INT(tb_col(&tb), 1);
}
static void set_failure_keeps_sticky_column(void) {
    fresh();
    puts_tb("abcdef\nab\nabcdef");
    tb_set_cursor(&tb, 5);
    tb_down(&tb);                               /* want = 6 */
    {
        char big[200];
        memset(big, 'z', sizeof big - 1);
        big[sizeof big - 1] = 0;
        CHECK(tb_set(&tb, big) != 0);
    }
    CHECK_EQ_INT(tb_cursor(&tb), 9);
    tb_down(&tb);
    CHECK_EQ_INT(tb_col(&tb), 6);
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
    failed_insert_keeps_sticky_column();
    goto_line_zero_is_line_one();
    set_failure_keeps_sticky_column();
    differential_random();
    TEST_MAIN_END();
}
