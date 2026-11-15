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
 *   - num_parse and num_format (assembly, num_c64.s) convert exactly, not
 *     with ROM FIN/FOUT, and num_format follows the host rules byte for byte.
 * Every pointer handed to the ROM points below $A000 (the caller's num_t
 * values); never pass BASICBSS data to a ROM operation. num_parse and
 * num_format never call the ROM, so their strings may live anywhere.
 * Nothing here is reentrant (static locals keep the code small). */
#include "platform/num.h"

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

#define NEG(n) ((n)->b[1] & 0x80)

/* File-static working copy and scratch: smaller cc65 code than locals. */
static num_t fa;
static uint8_t n;

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
 * significant digits prints back unchanged, as on the host.
 *
 * num_parse and num_format are written in assembly (num_c64.s): the same
 * code in cc65 C was about 2 KB larger. The methods:
 *
 * num_parse: same syntax as the host (strtod on [0-9.eE+-] with a full
 * match): [+-] digits [. digits] [(e|E) [+-] digits], at least one mantissa
 * digit. The lexer passes only digits and '.' (at most 16 characters).
 * Limits (the host has none): 32 characters, 24 significant digits; longer
 * input gives -1. Out of range: -1, as the host does for inf; below the
 * smallest C64 value (about 2.9E-39): 0. D = the significant digits as an
 * integer, value = D * 10^e. e > 0: D * 10^e exactly. e < 0:
 * floor(D * 2^k / 5^-e) * 2^(e-k) with k chosen so at least 34 bits
 * remain, `sticky` noting any lost remainder. Then round half to even to a
 * 32-bit mantissa.
 *
 * num_format: the host rules (src/host/num_host.c): 9 significant digits g
 * (value = 0.g * 10^x), trailing zeros removed; plain notation for
 * 1e-4 <= |a| < 1e9 (integers without a point), else g.gggE+xx. On the C64
 * the exponent never has 3 digits, so the host's 8-digit reduction for
 * 3-digit exponents cannot occur. Longest output: 15 chars + NUL
 * ("-1.23456789E+38", "-0.000123456789"). |a| = M * 2^(K-32), M the 32-bit
 * mantissa, K = exponent - $80, so |a| lies in [2^(K-1), 2^K). fl estimates
 * floor((K-1) log10 2), never too high and at most 4 too low (k/4 + k/32 +
 * k/64 for k = K-1 >= 0, -(n/4 + n/16 + 2) for n = 1-K), and s = 8 - fl.
 * Q = floor(2 |a| 10^s) = floor(M 5^s 2^(K-31+s)) has at least 9 digits
 * after halving; extra digits are divided off with `sticky` recording every
 * lost remainder, and the half bit rounds to even. A value whose 9 digits
 * round to exactly 1E-04 uses E form only if it lies below C64(1e-4) =
 * 73 51 B7 17 59, the C64 value nearest 1e-4 (above it). */


