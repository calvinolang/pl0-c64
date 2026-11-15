/* Host (double) implementation of the number layer. */
#include "platform/num.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void num_from_int(num_t *r, int16_t v) { *r = (double)v; }

int num_parse(const char *s, num_t *r)
{
    const char *p;
    char *end;
    double v;
    if (!s || !*s) return -1;
    for (p = s; *p; p++)
        if (!((*p >= '0' && *p <= '9') || *p == '.' || *p == 'e' || *p == 'E' ||
              *p == '+' || *p == '-'))
            return -1;
    v = strtod(s, &end);
    if (end == s || *end != '\0' || isnan(v) || isinf(v)) return -1;
    *r = v;
    return 0;
}

void num_add(num_t *r, const num_t *a, const num_t *b) { *r = *a + *b; }
void num_sub(num_t *r, const num_t *a, const num_t *b) { *r = *a - *b; }
void num_mul(num_t *r, const num_t *a, const num_t *b) { *r = *a * *b; }

int num_div(num_t *r, const num_t *a, const num_t *b)
{
    if (*b == 0.0) return 1;
    *r = *a / *b;
    return 0;
}

void num_neg(num_t *r, const num_t *a) { *r = -*a; }
int num_cmp(const num_t *a, const num_t *b) { return (*a > *b) - (*a < *b); }
int num_is_zero(const num_t *a) { return *a == 0.0; }

int num_is_odd(const num_t *a)
{
    double f = floor(*a);
    return fmod(f, 2.0) != 0.0;
}

int16_t num_to_i16(const num_t *a)
{
    double f = floor(*a);
    if (isnan(f)) return 0;
    if (f >= 32767.0) return 32767;
    if (f <= -32768.0) return -32768;
    return (int16_t)f;
}

int num_fn(uint8_t fn, num_t *r, const num_t *a)
{
    double x = *a, v;
    switch (fn) {
    case NUMFN_SIN: v = sin(x); break;
    case NUMFN_COS: v = cos(x); break;
    case NUMFN_ATN: v = atan(x); break;
    case NUMFN_SQR: if (x < 0.0) return 1; v = sqrt(x); break;
    case NUMFN_ABS: v = fabs(x); break;
    case NUMFN_INT: v = floor(x); break;
    case NUMFN_EXP: v = exp(x); break;
    case NUMFN_LOG: if (x <= 0.0) return 1; v = log(x); break;
    default: return 1;
    }
    if (isnan(v) || isinf(v)) return 1;
    *r = v;
    return 0;
}

/* E-style output with trailing mantissa zeros removed. Writes at most 15
 * chars + NUL: sign(1) + d.(2) + up to 8 more digits is only possible with a
 * 2-digit exponent (worst "-1.23456789E+99" = 15); a 3-digit exponent
 * ("E+308") gets 8 significant digits ("-1.2345679E+308" = 15). */
static void fmt_e(double x, char *buf)
{
    char t[40], *e, *dot;

    snprintf(t, sizeof t, "%.8E", x);
    e = strchr(t, 'E');
    if (e && strlen(e) > 4) {              /* 3-digit exponent: shorten mantissa */
        snprintf(t, sizeof t, "%.7E", x);
        e = strchr(t, 'E');
    }
    if (!e) { buf[0] = '\0'; return; }     /* unreachable for finite x */
    dot = e;
    while (dot > t && dot[-1] == '0') dot--;
    if (dot > t && dot[-1] == '.') dot--;
    memmove(dot, e, strlen(e) + 1);
    memcpy(buf, t, strlen(t) + 1);         /* strlen(t) <= 15 by construction */
}

void num_format(const num_t *a, char *buf)
{
    double x = *a, m = fabs(x);
    char t[40];

    if (x == 0.0) { strcpy(buf, "0"); return; }
    if (m >= 1e9 || m < 1e-4) { fmt_e(x, buf); return; }
    if (x == floor(x)) { snprintf(buf, 16, "%.0f", x); return; }
    snprintf(t, sizeof t, "%.9g", x);   /* %g already strips trailing zeros */
    if (strchr(t, 'e')) { fmt_e(x, buf); return; }  /* rounded out of range */
    t[15] = '\0';                        /* defensive; max is "-0.123456789" */
    strcpy(buf, t);
}
