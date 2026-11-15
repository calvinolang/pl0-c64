/* cc65 2.18 -O miscompile: `cc65 -t c64 -O tests/c64/cc65_bug_ternary.c -o /tmp/t.s`; WRONG output: after `cmp #$65` the false branch is `lda (sp),y` with y = 4 (the parameter r) instead of 2 (the local c); without -O it is right. Not built by any target. */
extern unsigned char get(void);
static char buf[8];

int f(char *r)
{
    unsigned char n = 0, c;
    const char *p = buf;
    c = get();
    buf[n++] = c == 0x65 ? 0x45 : c;     /* the miscompiled line */
    return p[0] + *r;
}
