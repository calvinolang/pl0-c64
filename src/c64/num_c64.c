/* C64 implementation of the number layer (src/platform/num.h) on the
 * BASIC V2 ROM floating point, through the shims in num_c64.s.
 *
 * num_t is the ROM's packed 5-byte float: b[0] = exponent (0 means the
 * value is 0, whatever the other bytes hold), b[1] bit 7 = sign, b[1..4] =
 * mantissa with the leading 1 implied: |v| = 0.1mmm...b * 2^(b[0] - $80).
 * Semantics follow src/host/num_host.c:
 *   - r may alias an operand: every operand is read into the ROM's FAC/ARG
 *     before the result is stored;
 *   - overflow (ROM error 15) is trapped by the shim and reported; nothing
 *     reaches BASIC's error handler;
 *   - domain errors (x/0, sqr of a negative, log of <= 0) are detected
 *     here before the ROM would raise them;
 *   - num_parse and num_format convert exactly (not with ROM FIN/FOUT),
 *     and num_format follows the host formatting rules byte for byte.
 * Every pointer handed to the ROM points below $A000 (the caller's num_t
 * values); never pass BASICBSS data to a ROM operation. num_parse and
 * num_format never call the ROM, so their strings may live anywhere.
 * Nothing here is reentrant (static locals keep the code small). */
#include "platform/num.h"
#include <string.h>

#pragma static-locals (on)

/* Shims (num_c64.s). Operation numbers 0..7 are the NUMFN_* codes. */
enum {
    OP_LOAD = 8,    /* FAC = *p           MOVFM  */
    OP_STORE,       /* *p = FAC (rounded) MOVMF  */
    OP_ADD,         /* FAC = *p + FAC     FADD   */
    OP_SUB,         /* FAC = *p - FAC     FSUB   */
    OP_MUL,         /* FAC = *p * FAC     FMULT  */
    OP_DIV,         /* FAC = *p / FAC     FDIV   */
    OP_CMP,         /* fp_a = sign(FAC - *p)     FCOMP  */
    OP_TOINT,       /* fp_a:fp_y = floor(FAC)    FACINX */
    OP_FROMINT      /* FAC = (int)p              GIVAYF */
};
#define ROM_OVERFLOW 15

uint8_t __fastcall__ fp_call(uint8_t op, const void *p);
extern uint8_t fp_a, fp_y;

/* Bignum (num_c64.s): little-endian unsigned integer of blen bytes. */
extern uint8_t big[32], blen;
void __fastcall__ big_muladd(uint16_t am);   /* big = big * (am & 255) + (am >> 8) */
uint8_t __fastcall__ big_div(uint8_t d);     /* big /= d; returns the remainder */
void __fastcall__ big_shl(uint8_t t);
uint8_t __fastcall__ big_shr(uint8_t t);     /* nonzero: a 1 bit was lost */
#define MULADD(m, a) big_muladd((uint16_t)(a) << 8 | (m))

#define NEG(n) ((n)->b[1] & 0x80)

/* File-static state instead of locals and parameters: cc65 code is much
 * smaller that way. */
static num_t fa;                    /* working copy of the operand */
static uint8_t i, n, nd, sticky, neg, dot;
static int8_t e, x;
static int16_t bexp;
static const char *ip;              /* parse input */
static char c;                      /* parse: current character */
static char *out;                   /* format output */

/* r = mem op fac. Returns 0, or 2 on overflow (also in the rounding store). */
static int binop(uint8_t op, num_t *r, const num_t *mem, const num_t *fac)
{
    fp_call(OP_LOAD, fac);
    if (fp_call(op, mem) || fp_call(OP_STORE, r)) return 2;
    return 0;
}

void num_from_int(num_t *r, int16_t v)
{
    fp_call(OP_FROMINT, (const void *)v);
    fp_call(OP_STORE, r);
}

int num_add(num_t *r, const num_t *a, const num_t *b) { return binop(OP_ADD, r, b, a); }
int num_sub(num_t *r, const num_t *a, const num_t *b) { return binop(OP_SUB, r, a, b); }
int num_mul(num_t *r, const num_t *a, const num_t *b) { return binop(OP_MUL, r, b, a); }

int num_div(num_t *r, const num_t *a, const num_t *b)
{
    if (!b->b[0]) return 1;
    return binop(OP_DIV, r, a, b);
}

int num_neg(num_t *r, const num_t *a)
{
    num_copy(r, a);
    if (r->b[0]) r->b[1] ^= 0x80;
    return 0;
}

int num_cmp(const num_t *a, const num_t *b)
{
    fp_call(OP_LOAD, a);
    fp_call(OP_CMP, b);
    return (int8_t)fp_a;
}

int num_is_zero(const num_t *a) { return !a->b[0]; }

/* Parity of floor(a): floor with ROM INT, then test the units bit. With
 * K = exponent - $80 the value lies in [2^(K-1), 2^K), so the units bit is
 * mantissa bit K-1 counted from the top (bit 0 being the implied 1). */
int num_is_odd(const num_t *a)
{
    if (!a->b[0] || a->b[0] > 0x80 + 32) return 0;   /* 0, or |a| >= 2^32: even */
    fp_call(OP_LOAD, a);
    fp_call(NUMFN_INT, 0);
    fp_call(OP_STORE, &fa);
    if (!fa.b[0]) return 0;
    n = fa.b[0] - 0x81;                                 /* bit index from the top */
    if (!n) return 1;                                   /* |floor| = 1 */
    return (fa.b[1 + (n >> 3)] & (0x80 >> (n & 7))) != 0;
}

/* floor, saturating: |a| >= 2^15 (exponent $90 and up) saturates. */
int16_t num_to_i16(const num_t *a)
{
    num_copy(&fa, a);
    if (!fa.b[0]) return 0;
    if (fa.b[0] >= 0x90) return NEG(&fa) ? -32767 - 1 : 32767;
    fp_call(OP_LOAD, &fa);
    fp_call(OP_TOINT, 0);
    return (int16_t)((uint16_t)fp_a << 8 | fp_y);
}

int num_fn(uint8_t fn, num_t *r, const num_t *a)
{
    num_copy(&fa, a);
    if (fn > NUMFN_LOG) return 1;
    if (fn == NUMFN_SQR && fa.b[0] && NEG(&fa)) return 1;
    if (fn == NUMFN_LOG && (!fa.b[0] || NEG(&fa))) return 1;
    fp_call(OP_LOAD, &fa);
    n = fp_call(fn, 0);
    if (!n) n = fp_call(OP_STORE, r);
    if (!n) return 0;
    return n == ROM_OVERFLOW ? 2 : 1;
}

/* ---- exact decimal <-> binary conversion --------------------------------
 * ROM FIN and FOUT are not used. FIN divides by 10 once per fractional digit
 * and FOUT scales by repeated x10 and /10, so both lose the last bits: FIN
 * reads "0.000099" a few units low (prints 9.89999999E-05), and FOUT prints
 * pi (packed 82 49 0F DA A2 = 3.14159265346) as 3.14159266 and 2^-20 as
 * 9.53674317E-07. Both directions are done here with exact integer
 * arithmetic on the bignum and round half to even, so a literal parses to
 * the nearest C64 value and every C64 value prints exactly as the host
 * rules print the same value. Since 10^9 < 2^31, any literal of up to 9
 * significant digits prints back unchanged, as on the host. */

static uint8_t big_bits(void)       /* bit length of big (at most 226 here) */
{
    uint8_t top, b;
    if (!blen) return 0;
    b = (blen - 1) << 3;
    for (top = big[blen - 1]; top; top >>= 1) ++b;
    return b;
}

static void adv(void) { c = *++ip; }
static uint8_t is_dig(void) { return (uint8_t)(c - '0') < 10; }

static uint8_t sign(void)          /* skips + or -; returns 1 for - */
{
    if (c == '-') { adv(); return 1; }
    if (c == '+') adv();
    return 0;
}

/* Same syntax as the host (strtod on [0-9.eE+-] with a full match):
 * [+-] digits [. digits] [(e|E) [+-] digits], at least one mantissa digit.
 * The lexer passes only digits and '.' (at most 16 characters). Limits
 * here (the host has none): 32 characters, 24 significant digits; longer
 * input gives -1. Out of range: -1, as the host does for inf; below the
 * smallest C64 value (about 2.9E-39): 0.
 * Method: D = the significant digits as an integer, value = D * 10^e.
 * e > 0: D * 10^e exactly. e < 0: floor(D * 2^k / 5^-e) * 2^(e-k) with k
 * chosen so at least 34 bits remain; `sticky` notes any lost remainder.
 * Then the result is rounded half to even to a 32-bit mantissa. */
int num_parse(const char *s, num_t *r)
{
    uint8_t dig, ex;

    if (strlen(s) > 32) return -1;
    ip = s;
    c = *s;
    dig = nd = dot = sticky = blen = 0;
    e = 0;
    bexp = 0;
    neg = sign();
    for (;; adv()) {
        if (is_dig()) {
            dig = 1;
            if (blen || c != '0') {
                if (nd == 24) return -1;
                MULADD(10, c - '0');
                ++nd;
            }
            if (dot) --e;                               /* e >= -32 */
        } else if (c == '.' && !dot) dot = 1;
        else break;
    }
    if (!dig) return -1;
    if (c == 'e' || c == 'E') {
        adv();
        x = sign();
        if (!is_dig()) return -1;
        for (ex = 0; is_dig(); adv())
            if (ex < 100) ex = ex * 10 + (c - '0');
        if (ex > 80) {                                  /* far out of range */
            if (!blen) goto zero;
            if (x) goto zero;
            return -1;
        }
        e += x ? -(int8_t)ex : (int8_t)ex;              /* -112..80 */
    }
    if (c) return -1;
    if (!blen) goto zero;
    if (e + (int8_t)nd > 40) return -1;                 /* value >= 1e40 */
    if (e + (int8_t)nd <= -40) goto zero;               /* value < 1e-40 */
    if (e > 0) {
        for (n = e; n; --n) MULADD(10, 0);
    } else if (e) {
        i = -e;                                         /* 5^i < 2^(3i) */
        n = 34 + 3 * i;                                 /* keep >= 34 bits */
        dig = big_bits();
        if (n > dig) n -= dig; else n = 0;
        big_shl(n);
        bexp = -(int16_t)(n + i);
        for (; i >= 3; i -= 3) sticky |= big_div(125);
        for (; i; --i) sticky |= big_div(5);
    }
    n = big_bits();                                     /* normalise to 33 bits */
    if (n > 33) sticky |= big_shr(n - 33);
    else big_shl(33 - n);
    bexp += n;
    if (big_shr(1) && (sticky || (big[0] & 1))) {       /* half bit: to even */
        MULADD(1, 1);
        if (blen > 4) { big_shr(1); ++bexp; }           /* carried to 2^32 */
    }
    bexp += 0x80;                                       /* C64 exponent byte */
    if (bexp > 255) return -1;
    if (bexp < 1) goto zero;
    fa.b[0] = (uint8_t)bexp;
    fa.b[1] = (big[3] & 0x7F) | (neg ? 0x80 : 0);
    fa.b[2] = big[2];
    fa.b[3] = big[1];
    fa.b[4] = big[0];
    num_copy(r, &fa);
    return 0;
zero:
    memset(r, 0, sizeof *r);
    return 0;
}

/* ---- num_format ------------------------------------------------------- */

/* The C64 value nearest to 1e-4 (73 51 B7 17 59) lies slightly above 1e-4,
 * so "magnitude below it" equals the host's "below 1e-4". Needed only when
 * the 9 rounded digits are exactly 1E-04. */
static const uint8_t ten_m4[5] = { 0x73, 0x51, 0xB7, 0x17, 0x59 };
static const uint8_t two_e9[4] = { 0x00, 0x94, 0x35, 0x77 };   /* 2e9, LE */
static char d[10];

static uint8_t below_1e4(void)
{
    fa.b[1] &= 0x7F;
    for (i = 0; i < 5; ++i)
        if (fa.b[i] != ten_m4[i]) return fa.b[i] < ten_m4[i];
    return 0;
}

static uint8_t big_ge_2e9(void)
{
    if (blen != 4) return blen > 4;
    for (i = 4; i--; )
        if (big[i] != two_e9[i]) return big[i] > two_e9[i];
    return 1;
}

static void emit(char c) { *out++ = c; }

/* The host rules (src/host/num_host.c): 9 significant digits g (value =
 * 0.g * 10^x), trailing zeros removed; plain notation for 1e-4 <= |a| < 1e9
 * (integers without a point), else g.gggE+xx. On the C64 the exponent
 * never has 3 digits. Longest output: 15 chars + NUL ("-1.23456789E+38",
 * "-0.000123456789").
 *
 * |a| = M * 2^(K-32), M the 32-bit mantissa, K = exponent - $80, so |a| lies
 * in [2^(K-1), 2^K). fl estimates floor((K-1) log10 2), never too high and
 * at most 4 too low, and s = 8 - fl. Q = floor(2 |a| 10^s) =
 * floor(M 5^s 2^(K-31+s)) has at least 9 digits after halving; extra digits
 * are divided off with `sticky` recording every lost remainder, and the
 * half bit rounds to even. */
void num_format(const num_t *a, char *buf)
{
    const char *g;

    out = buf;
    num_copy(&fa, a);
    if (!fa.b[0]) { emit('0'); goto end; }
    if (NEG(&fa)) emit('-');
    if (fa.b[0] >= 0x81) {                              /* k = K - 1 >= 0 */
        n = fa.b[0] - 0x81;
        e = 8 - ((n >> 2) + (n >> 5) + (n >> 6));      /* 0.297 k <= k log10 2 */
    } else {
        n = 0x81 - fa.b[0];                             /* -k */
        e = 10 + (n >> 2) + (n >> 4);                  /* 0.3125 (-k) + 2 */
    }
    big[3] = fa.b[1] | 0x80; big[2] = fa.b[2]; big[1] = fa.b[3]; big[0] = fa.b[4];
    blen = 4;
    sticky = 0;
    x = (int8_t)(fa.b[0] - 0x81) - 30 + e;             /* binary exponent of Q */
    for (i = e; (int8_t)i > 0; --i) MULADD(5, 0);
    if (x > 0) big_shl(x);
    else if (x) sticky = big_shr(-x);
    for (i = -e; (int8_t)i > 0; --i) sticky |= big_div(5);
    while (big_ge_2e9()) {                              /* 10+ digits: drop one */
        sticky |= big_div(10);
        --e;
    }
    if (big_shr(1) && (sticky || (big[0] & 1)))         /* half bit: to even */
        MULADD(1, 1);
    for (i = 10; i--; ) d[i] = '0' + big_div(10);
    x = 9 - e;
    g = d + 1;
    if (d[0] != '0') { g = d; ++x; }                    /* rounded up to 10^9 */
    for (n = 9; n > 1 && g[n - 1] == '0'; --n) ;
    if (x >= 10 || x <= -4 || (x == -3 && n == 1 && g[0] == '1' && below_1e4())) {
        emit(g[0]);
        if (n > 1) {
            emit('.');
            for (i = 1; i < n; ++i) emit(g[i]);
        }
        emit('E');
        --x;
        if (x < 0) { emit('-'); x = -x; } else emit('+');
        emit('0' + x / 10);
        emit('0' + x % 10);
    } else if (x <= 0) {
        emit('0');
        emit('.');
        for (; x < 0; ++x) emit('0');
        for (i = 0; i < n; ++i) emit(g[i]);
    } else {
        for (i = 0; i < n || i < (uint8_t)x; ++i) {
            if (i == (uint8_t)x) emit('.');
            emit(i < n ? g[i] : '0');
        }
    }
end:
    *out = 0;
}
