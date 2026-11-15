/* Turtle graphics on top of the number layer and gfx_line.
 *
 * Speed (Task 1 spike: ROM sin is about 28 ms): sin and cos of the heading
 * are cached and recomputed only when the heading changes (turn by a
 * nonzero angle). Heading 0 (start, home) uses preset values 0 and 1.
 * Position and heading are kept as numbers; only line endpoints are rounded
 * (to the NEAREST pixel: floor(v + 0.5), which num_to_i16 provides after
 * adding 0.5), so closed figures close on their start pixel.
 *
 * num_to_i16 saturates, which would bend a line whose far end is off by
 * more than 32767 pixels (both ends clamp to the same value, so the slope
 * changes). When any converted endpoint saturated, the line is first clipped
 * in number arithmetic (Liang-Barsky) to a +-8000 pixel box that contains
 * the screen; the result is then well inside int16. */
#include "core/turtle.h"
#include "platform/gfx.h"
#include "core/ascii.h"     /* the constant literals are ASCII */

static num_t tx, ty;            /* position (turtle coordinates) */
static num_t th;                /* heading in degrees, 0 <= th < 360 */
static num_t ts, tc;            /* sin and cos of the heading */
static uint8_t pen;
static uint8_t used;

static num_t k360, kpi180, k160h, k99h, kbox, kzero, kone, kbig;
static num_t nx, ny;            /* scratch: new position */
static num_t pa[4];             /* line in pixel space: ax, ay, bx, by */
static num_t t0, t1, ddx, ddy;  /* Liang-Barsky scratch */

#ifdef PL0_TEST_HOOKS
static unsigned trig;
unsigned turtle_trig_count(void) { return trig; }
#endif

static void set_heading0(void)
{
    num_from_int(&th, 0);
    num_from_int(&ts, 0);
    num_from_int(&tc, 1);
}

/* Constants are parsed once per program start (each parse is a ROM call on
 * the C64). kbig = 1e6 squared twice = 1e24, built without an exponent
 * literal (num_parse may not accept one). */
static uint8_t inited;

static void init_constants(void)
{
    num_parse("360", &k360);
    num_parse("0.017453292519943", &kpi180);
    num_parse("160.5", &k160h);
    num_parse("99.5", &k99h);
    num_parse("8000", &kbox);
    num_parse("1000000", &kbig);
    (void)num_mul(&kbig, &kbig, &kbig);
    (void)num_mul(&kbig, &kbig, &kbig);
    num_from_int(&kzero, 0);
    num_from_int(&kone, 1);
    inited = 1;
}

void turtle_reset(void)
{
    if (!inited) init_constants();
    num_from_int(&tx, 0);
    num_from_int(&ty, 0);
    set_heading0();
    pen = 1;
    used = 0;
#ifdef PL0_TEST_HOOKS
    trig = 0;
#endif
}

int turtle_used(void) { return used; }

static void touch(void)
{
    if (!used) {
        used = 1;
        gfx_open();
    }
}

/* Heading reduced into [0, 360) without loops: h - floor(h/360)*360. */
static void reduce_heading(void)
{
    num_t q;
    int bad = 0;
    bad |= num_div(&q, &th, &k360);
    bad |= num_fn(NUMFN_INT, &q, &q);
    bad |= num_mul(&q, &q, &k360);
    bad |= num_sub(&th, &th, &q);
    /* huge angles lose precision (about 9 digits): keep the result bounded */
    if (bad || num_cmp(&th, &kzero) < 0 || num_cmp(&th, &k360) >= 0) num_from_int(&th, 0);
}

int turtle_turn(const num_t *deg)
{
    num_t r;
    touch();
    if (num_is_zero(deg)) return 0;
    if (num_add(&th, &th, deg)) return 1;
    reduce_heading();
    if (num_mul(&r, &th, &kpi180)) return 1;
    (void)num_fn(NUMFN_SIN, &ts, &r);
    (void)num_fn(NUMFN_COS, &tc, &r);
#ifdef PL0_TEST_HOOKS
    trig += 2;
#endif
    return 0;
}

/* One edge of Liang-Barsky: p = direction term, q = distance to the edge.
 * Returns 1 if the line lies outside (rejected). */
static int lb_edge(const num_t *p, const num_t *q)
{
    num_t r;
    int c;
    if (num_is_zero(p)) return num_cmp(q, &kzero) < 0;
    c = num_div(&r, q, p);
    if (c) {                                /* overflow: |r| is huge */
        num_copy(&r, &kbig);
        if ((num_cmp(q, &kzero) < 0) != (num_cmp(p, &kzero) < 0)) (void)num_neg(&r, &r);
    }
    if (num_cmp(p, &kzero) < 0) {
        if (num_cmp(&r, &t1) > 0) return 1;
        if (num_cmp(&r, &t0) > 0) num_copy(&t0, &r);
    } else {
        if (num_cmp(&r, &t0) < 0) return 1;
        if (num_cmp(&r, &t1) < 0) num_copy(&t1, &r);
    }
    return 0;
}

/* Clip pa[] to the +-kbox square (pixel space). Returns 1 if nothing is
 * left, -1 on overflow, 0 ok (pa updated). */
static int far_clip(void)
{
    num_t p, q, nb;
    num_t ex, ey;
    num_copy(&t0, &kzero);
    num_copy(&t1, &kone);
    (void)num_neg(&nb, &kbox);
    if (num_sub(&ddx, &pa[2], &pa[0]) || num_sub(&ddy, &pa[3], &pa[1])) return -1;
    (void)num_neg(&p, &ddx);
    if (num_sub(&q, &pa[0], &nb)) return -1;
    if (lb_edge(&p, &q)) return 1;
    if (num_sub(&q, &kbox, &pa[0])) return -1;
    if (lb_edge(&ddx, &q)) return 1;
    (void)num_neg(&p, &ddy);
    if (num_sub(&q, &pa[1], &nb)) return -1;
    if (lb_edge(&p, &q)) return 1;
    if (num_sub(&q, &kbox, &pa[1])) return -1;
    if (lb_edge(&ddy, &q)) return 1;
    if (num_mul(&ex, &ddx, &t1) || num_mul(&ey, &ddy, &t1)) return -1;
    if (num_add(&pa[2], &pa[0], &ex) || num_add(&pa[3], &pa[1], &ey)) return -1;
    if (num_mul(&ex, &ddx, &t0) || num_mul(&ey, &ddy, &t0)) return -1;
    if (num_add(&pa[0], &pa[0], &ex) || num_add(&pa[1], &pa[1], &ey)) return -1;
    return 0;
}

static int saturated(int16_t v) { return (uint16_t)v == 0x7FFFu || (uint16_t)v == 0x8000u; }

/* Draw pa[] (pixel space floats already including the +0.5 rounding). */
static int draw_pa(void)
{
    int16_t x0 = num_to_i16(&pa[0]), y0 = num_to_i16(&pa[1]);
    int16_t x1 = num_to_i16(&pa[2]), y1 = num_to_i16(&pa[3]);
    int c;
    if (saturated(x0) || saturated(y0) || saturated(x1) || saturated(y1)) {
        c = far_clip();
        if (c < 0) return 1;
        if (c > 0) return 0;
        x0 = num_to_i16(&pa[0]); y0 = num_to_i16(&pa[1]);
        x1 = num_to_i16(&pa[2]); y1 = num_to_i16(&pa[3]);
    }
    gfx_line(x0, y0, x1, y1);
    return 0;
}

/* pixel-space float of a turtle point: x + 160.5, 99.5 - y */
static int to_pixel(num_t *px, num_t *py, const num_t *x, const num_t *y)
{
    return num_add(px, x, &k160h) | num_sub(py, &k99h, y);
}

/* Move to (nx, ny), drawing if the pen is down. State is unchanged on error. */
static int move_new(void)
{
    if (pen) {
        if (to_pixel(&pa[0], &pa[1], &tx, &ty)) return 1;
        if (to_pixel(&pa[2], &pa[3], &nx, &ny)) return 1;
        if (draw_pa()) return 1;
    }
    num_copy(&tx, &nx);
    num_copy(&ty, &ny);
    return 0;
}

int turtle_forward(const num_t *d)
{
    num_t t;
    touch();
    if (num_mul(&t, d, &ts) || num_add(&nx, &tx, &t)) return 1;
    if (num_mul(&t, d, &tc) || num_add(&ny, &ty, &t)) return 1;
    return move_new();
}

int turtle_moveto(const num_t *x, const num_t *y)
{
    touch();
    num_copy(&nx, x);
    num_copy(&ny, y);
    return move_new();
}

void turtle_penup(void) { touch(); pen = 0; }
void turtle_pendown(void) { touch(); pen = 1; }

void turtle_home(void)
{
    touch();
    num_from_int(&tx, 0);
    num_from_int(&ty, 0);
    set_heading0();
}
