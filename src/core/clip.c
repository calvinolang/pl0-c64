/* Cohen-Sutherland line clipping to the 320x200 bitmap.
 *
 * Intersections are always computed from the ORIGINAL endpoints (not the
 * already moved ones), so errors do not accumulate. Arithmetic uses 32-bit
 * longs; the products dx*(edge-y0) reach 65535 * 32967 (about 2.16e9), which
 * exceeds a signed 32-bit long, so mul_div works on unsigned magnitudes
 * (max 4.29e9) and restores the sign afterwards. */
#include "core/clip.h"

#define XMAX 319
#define YMAX 199

enum { OUT_L = 1, OUT_R = 2, OUT_T = 4, OUT_B = 8 };

static uint8_t outcode(int16_t x, int16_t y)
{
    uint8_t c = 0;
    if (x < 0) c |= OUT_L;
    else if (x > XMAX) c |= OUT_R;
    if (y < 0) c |= OUT_T;
    else if (y > YMAX) c |= OUT_B;
    return c;
}

/* round(a * b / d) for d != 0, |a|,|d| <= 65535, |b| <= 32967 */
static long mul_div(long a, long b, long d)
{
    unsigned long ua, ub, ud, r;
    int neg = 0;
    if (a < 0) { ua = (unsigned long)-a; neg = !neg; } else ua = (unsigned long)a;
    if (b < 0) { ub = (unsigned long)-b; neg = !neg; } else ub = (unsigned long)b;
    if (d < 0) { ud = (unsigned long)-d; neg = !neg; } else ud = (unsigned long)d;
    r = (ua * ub + ud / 2) / ud;
    if (neg) return -(long)r;      /* if, not ?: on locals (cc65 -O bug) */
    return (long)r;
}

int clip_line(int16_t *px0, int16_t *py0, int16_t *px1, int16_t *py1)
{
    int16_t x0 = *px0, y0 = *py0, x1 = *px1, y1 = *py1;
    int16_t ox = x0, oy = y0;
    long dx = (long)x1 - x0, dy = (long)y1 - y0;
    uint8_t c0, c1, c;
    uint8_t n;
    int16_t x, y;

    for (n = 0; n < 8; n++) {
        c0 = outcode(x0, y0);
        c1 = outcode(x1, y1);
        if (!(c0 | c1)) {
            *px0 = x0; *py0 = y0; *px1 = x1; *py1 = y1;
            return 1;
        }
        if (c0 & c1) return 0;
        c = c1;                    /* not c0 ? c0 : c1 (cc65 -O bug) */
        if (c0) c = c0;
        if (c & OUT_B)      { y = YMAX; x = (int16_t)(ox + mul_div(dx, (long)YMAX - oy, dy)); }
        else if (c & OUT_T) { y = 0;    x = (int16_t)(ox + mul_div(dx, -(long)oy, dy)); }
        else if (c & OUT_R) { x = XMAX; y = (int16_t)(oy + mul_div(dy, (long)XMAX - ox, dx)); }
        else                { x = 0;    y = (int16_t)(oy + mul_div(dy, -(long)ox, dx)); }
        if (c == c0) { x0 = x; y0 = y; }
        else         { x1 = x; y1 = y; }
    }
    return 0;       /* did not converge (line grazes a corner): draw nothing */
}
