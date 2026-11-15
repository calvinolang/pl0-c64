/* numw.c -- SPIKE weight stub: the num.h API on top of fp.s (approximates
 * the weight of Task 11's num_c64.c; formatting reduced to FOUT). */
#include <string.h>
#include "weight.h"

void num_from_int(num_t *r, int16_t v) { fp_from_int(v); fp_store(r); }
int  num_parse(const char *s, num_t *r)
{
    if (fp_parse(s)) return -1;
    fp_store(r);
    return 0;
}
void num_add(num_t *r, const num_t *a, const num_t *b) { fp_load(b); fp_add(a); fp_store(r); }
void num_sub(num_t *r, const num_t *a, const num_t *b) { fp_load(b); fp_sub(a); fp_store(r); }
void num_mul(num_t *r, const num_t *a, const num_t *b) { fp_load(b); fp_mul(a); fp_store(r); }
int  num_div(num_t *r, const num_t *a, const num_t *b)
{
    if (b->b[0] == 0) return 1;           /* exponent byte 0 <=> value 0 */
    fp_load(b);
    if (fp_div(a)) return 1;
    fp_store(r);
    return 0;
}
void num_neg(num_t *r, const num_t *a) { *r = *a; if (r->b[0]) r->b[1] ^= 0x80; }
int  num_is_zero(const num_t *a) { return a->b[0] == 0; }
/* compare in C on the packed format: sign, exponent, mantissa */
int  num_cmp(const num_t *a, const num_t *b)
{
    signed char sa = a->b[0] ? ((a->b[1] & 0x80) ? -1 : 1) : 0;
    signed char sb = b->b[0] ? ((b->b[1] & 0x80) ? -1 : 1) : 0;
    unsigned char i, x, y;
    if (sa != sb) return sa < sb ? -1 : 1;
    if (sa == 0) return 0;
    for (i = 0; i < 5; ++i) {
        x = a->b[i]; y = b->b[i];
        if (i == 1) { x &= 0x7f; y &= 0x7f; }
        if (x != y) return ((x < y) ? -1 : 1) * sa;
    }
    return 0;
}
int16_t num_to_i16(const num_t *a)
{
    int v;
    fp_load(a);
    fp_fn(5);                               /* INT = floor */
    v = fp_to_int();
    if (fp_lasterr) return (a->b[1] & 0x80) ? (-32767 - 1) : 32767;
    return (int16_t)v;
}
int  num_is_odd(const num_t *a) { return num_to_i16(a) & 1; }
int  num_fn(uint8_t fn, num_t *r, const num_t *a)
{
    if ((fn == NUMFN_SQR && (a->b[1] & 0x80) && a->b[0]) ||
        (fn == NUMFN_LOG && ((a->b[1] & 0x80) || !a->b[0])))
        return 1;
    fp_load(a);
    if (fp_fn(fn)) return 1;
    fp_store(r);
    return 0;
}
void num_format(const num_t *a, char *buf)
{
    const char *s;
    fp_load(a);
    s = fp_fout();
    if (*s == ' ') ++s;
    strcpy(buf, s);
}
