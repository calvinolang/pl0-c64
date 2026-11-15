/* main.c -- SPIKE (throwaway): prove BASIC ROM floats work from cc65.
 * Results go to the screen and to logbuf[] (plain ASCII), which run.sh
 * dumps from VICE when the program reaches spike_done(). */
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>

typedef struct { unsigned char b[5]; } num_t;

void __fastcall__ fp_load(const num_t *x);
unsigned char __fastcall__ fp_store(num_t *r);
unsigned char __fastcall__ fp_add(const num_t *x);   /* FAC = x + FAC */
unsigned char __fastcall__ fp_sub(const num_t *x);   /* FAC = x - FAC */
unsigned char __fastcall__ fp_mul(const num_t *x);
unsigned char __fastcall__ fp_div(const num_t *x);   /* FAC = x / FAC */
unsigned char __fastcall__ fp_fn(unsigned char fn);
const char *fp_fout(void);
void __fastcall__ fp_from_int(int v);
int fp_to_int(void);
signed char __fastcall__ fp_cmp(const num_t *x);
unsigned char __fastcall__ fp_parse(const char *s);
void fp_nop(void);
extern unsigned char fp_zpclobber[26];
extern unsigned char fp_lasterr;
extern char *fp_endptr;
extern unsigned char fp_instrument;

enum { F_SIN, F_COS, F_ATN, F_SQR, F_ABS, F_INT, F_EXP, F_LOG, F_NEG };

char logbuf[3072];
static unsigned logpos;

static void say(const char *fmt, ...)
{
    static char t[80];
    va_list ap;
    unsigned n;
    va_start(ap, fmt);
    vsprintf(t, fmt, ap);
    va_end(ap);
    puts(t);   /* KERNAL CHROUT: scrolls (cc65 conio does not) */
    n = strlen(t);
    if (logpos + n + 2 < sizeof logbuf) {
        memcpy(logbuf + logpos, t, n);
        logpos += n;
        logbuf[logpos++] = '\n';
        logbuf[logpos] = 0;
    }
}

/* run.sh breaks here and dumps logbuf */
void spike_done(void) { for (;;) ; }

static num_t one, two, three, zero, ten, big, minus1, x, r;

static void mk(num_t *n, const char *s) { fp_parse(s); fp_store(n); }

static void showfac(const char *label)
{
    num_t t;
    char s[20];
    fp_store(&t);
    strcpy(s, fp_fout());
    say("%s[%s] %02x %02x %02x %02x %02x", label, s,
        t.b[0], t.b[1], t.b[2], t.b[3], t.b[4]);
}

static void fn1(const char *label, const num_t *a, unsigned char fn)
{
    unsigned char e;
    fp_load(a);
    e = fp_fn(fn);
    if (e) say("%serr %u", label, e); else showfac(label);
}

#define N 500
static unsigned timeit(unsigned char which)
{
    clock_t t0;
    unsigned i;
    t0 = clock();
    for (i = 0; i < N; ++i) {
        switch (which) {
        case 0: fp_nop(); break;
        case 1: fp_load(&three); break;
        case 2: fp_load(&three); fp_add(&x); fp_store(&r); break;
        case 3: fp_load(&three); fp_mul(&x); fp_store(&r); break;
        case 4: fp_load(&three); fp_div(&x); fp_store(&r); break;
        case 5: fp_load(&x); fp_fn(F_SIN); fp_store(&r); break;
        case 6: fp_load(&x); fp_fn(F_SQR); fp_store(&r); break;
        case 7: fp_load(&x); fp_fout(); break;
        case 8: fp_load(&x); fp_cmp(&three); break;
        case 9: fp_from_int(1234); fp_store(&r); break;
        case 10: fp_parse("3.14159"); fp_store(&r); break;
        }
    }
    return (unsigned)(clock() - t0);
}

int main(void)
{
    unsigned char e, i;
    int iv;
    static const char *const tn[] = { "nop", "load", "ld+add+st", "ld+mul+st",
        "ld+div+st", "ld+sin+st", "ld+sqr+st", "ld+fout", "ld+cmp",
        "int->fp+st", "parse+st" };

    say("spike rom-float  $01=%02x", *(unsigned char *)1);
    fp_from_int(1); fp_store(&one);
    fp_from_int(2); fp_store(&two);
    fp_from_int(3); fp_store(&three);
    fp_from_int(0); fp_store(&zero);
    fp_from_int(10); fp_store(&ten);
    fp_from_int(-1); fp_store(&minus1);
    mk(&big, "1e38");

    /* the three required values */
    fp_load(&three); fp_div(&one); showfac("1/3     ");
    fn1("sqr(2)  ", &two, F_SQR);
    fn1("sin(1)  ", &one, F_SIN);
    /* the rest of num.h */
    fn1("cos(0)  ", &zero, F_COS);
    fp_load(&one); fp_fn(F_ATN); fp_store(&x);
    fp_from_int(4); fp_mul(&x); showfac("4*atn(1)");
    fn1("exp(1)  ", &one, F_EXP);
    fn1("log(10) ", &ten, F_LOG);
    mk(&x, "-2.5"); fn1("int(-2.5)", &x, F_INT);
    fn1("abs(-2.5)", &x, F_ABS);
    fn1("neg(-2.5)", &x, F_NEG);
    fp_load(&three); fp_sub(&one); showfac("1-3     ");
    fp_load(&two); fp_add(&three); showfac("3+2     ");
    fp_load(&three); fp_mul(&ten); showfac("10*3    ");
    /* formatting cases (compare with host num_format rules) */
    mk(&x, "0.1"); mk(&r, "0.2"); fp_load(&r); fp_add(&x); showfac("0.1+0.2 ");
    mk(&x, "1e9"); fp_load(&x); showfac("1e9     ");
    mk(&x, "123456789"); fp_load(&x); showfac("123456789");
    mk(&x, "0.001"); fp_load(&x); showfac("0.001   ");
    mk(&x, "0.01"); fp_load(&x); showfac("0.01    ");
    mk(&x, "3628800"); fp_load(&x); showfac("3628800 ");
    mk(&x, "-1.5e-7"); fp_load(&x); showfac("-1.5e-7 ");
    {
        static char s[] = "12.5xyz";
        e = fp_parse(s);
        say("parse 12.5xyz err=%u used=%u", e, (unsigned)(fp_endptr - s));
        showfac("  value ");
    }
    /* compare and int conversion */
    {
        signed char c1, c2, c3;
        fp_load(&two); c1 = fp_cmp(&three);
        fp_load(&three); c2 = fp_cmp(&three);
        fp_load(&ten); c3 = fp_cmp(&three);
        say("cmp(2,3)=%d cmp(3,3)=%d cmp(10,3)=%d", c1, c2, c3);
    }
    mk(&x, "-1234.7"); fp_load(&x);
    iv = fp_to_int(); say("toint(-1234.7)=%d err=%u", iv, fp_lasterr);

    /* errors: the ROM would print ?... ERROR and warm-start BASIC */
    fp_load(&zero); e = fp_div(&one);           say("1/0       err=%u", e);
    fn1("sqr(-1)  ", &minus1, F_SQR);
    fn1("log(0)   ", &zero, F_LOG);
    fn1("log(-1)  ", &minus1, F_LOG);
    fp_from_int(100); fp_store(&x); fn1("exp(100) ", &x, F_EXP);
    fp_load(&ten); e = fp_mul(&big);            say("1e38*10   err=%u", e);
    e = fp_parse("1e39");                       say("parse 1e39 err=%u", e);
    mk(&x, "40000"); fp_load(&x); iv = fp_to_int(); say("toint(40000) err=%u", fp_lasterr);
    mk(&x, "1e-39"); fp_load(&x); showfac("1e-39 (underflow)");
    mk(&x, "1e10"); fn1("sin(1e10)", &x, F_SIN);
    fp_load(&three); fp_div(&one); showfac("after errors 1/3");

    /* ROM writes into cc65's zero page $02-$1B */
    {
        char t[80]; unsigned p = 0;
        for (i = 0; i < 26; ++i)
            if (fp_zpclobber[i]) p += sprintf(t + p, "%02x ", i + 2);
        t[p] = 0;
        say("zp written: %s", t);
    }

    /* timing, instrumentation off: jiffies (1/60 s) for N=500 calls */
    fp_instrument = 0;
    mk(&x, "1.2345");
    fp_parse("1.2345"); fp_store(&x);
    for (i = 0; i < 11; ++i) {
        unsigned j = timeit(i);
        say("t %-10s %3u jif %5lu us/op", tn[i], j, (unsigned long)j * 1000000UL / (60UL * N));
    }
    say("done");
    spike_done();
    return 0;
}
