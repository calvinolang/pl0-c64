#include "test.h"
#include "core/compiler.h"
#include "core/vm.h"
#include "core/turtle.h"
#include "core/clip.h"
#include "core/errors.h"
#include "platform/gfx.h"
#include "host/gfx_host.h"
#include "host/io_host.h"
#include <stdio.h>
#include <string.h>

static Program prog;
static char result[256];

/* Compile and run; returns "" on success or "ERROR line N: msg". */
static const char *run(const char *src) {
    PL0Error e;
    io_capture_begin();
    gfx_host_reset();
    if (pl0_compile(src, &prog, &e) != 0 || pl0_run(&prog, &e) != 0) {
        sprintf(result, "ERROR line %u: %s", (unsigned)e.line, err_text(e.code));
        return result;
    }
    return io_capture_get();
}

static int nlines(void) { return gfx_host_line_count(); }
static void line_is(int i, int x0, int y0, int x1, int y1) {
    int16_t a, b, c, d;
    gfx_host_line(i, &a, &b, &c, &d);
    CHECK_EQ_INT(a, x0); CHECK_EQ_INT(b, y0); CHECK_EQ_INT(c, x1); CHECK_EQ_INT(d, y1);
}

static void forward_up(void) {
    CHECK_EQ_STR(run("begin home; forward(50) end."), "");
    CHECK_EQ_INT(nlines(), 1);
    line_is(0, 160, 99, 160, 49);
    CHECK_EQ_INT(turtle_used(), 1);
    CHECK_EQ_INT(gfx_host_is_open(), 1);
}
static void no_turtle_not_used(void) {
    CHECK_EQ_STR(run("! 1."), "1");
    CHECK_EQ_INT(turtle_used(), 0);
    CHECK_EQ_INT(gfx_host_is_open(), 0);
}
static void turn_right(void) {
    CHECK_EQ_STR(run("begin turn(90); forward(10) end."), "");
    CHECK_EQ_INT(nlines(), 1);
    line_is(0, 160, 99, 170, 99);
}
static void turn_negative(void) {
    CHECK_EQ_STR(run("begin turn(-90); forward(10) end."), "");
    line_is(0, 160, 99, 150, 99);
}
static void penup_no_line(void) {
    CHECK_EQ_STR(run("begin penup; forward(50); pendown; forward(10) end."), "");
    CHECK_EQ_INT(nlines(), 1);
    line_is(0, 160, 49, 160, 39);
}
static void home_resets_position_and_heading(void) {
    CHECK_EQ_STR(run("begin turn(90); penup; forward(30); home; pendown; forward(20) end."), "");
    CHECK_EQ_INT(nlines(), 1);
    line_is(0, 160, 99, 160, 79);
}
static void moveto_draws_when_pen_down(void) {
    CHECK_EQ_STR(run("begin moveto(10, 20); penup; moveto(0, 0); pendown; moveto(-5, -5) end."), "");
    CHECK_EQ_INT(nlines(), 2);
    line_is(0, 160, 99, 170, 79);
    line_is(1, 160, 99, 155, 104);
}
static void square(void) {
    int i;
    CHECK_EQ_STR(run("var i; begin i := 0; while i < 4 do begin forward(50); turn(90); i := i + 1 end end."), "");
    CHECK_EQ_INT(nlines(), 4);
    line_is(0, 160, 99, 160, 49);
    line_is(1, 160, 49, 210, 49);
    line_is(2, 210, 49, 210, 99);
    line_is(3, 210, 99, 160, 99);
    (void)i;
}
static void polygon_closes(void) {
    /* 7-gon: float heading 360/7 each time; last point must land on the start pixel */
    CHECK_EQ_STR(run("var i; begin i := 0; while i < 7 do begin forward(40); turn(360/7); i := i + 1 end end."), "");
    CHECK(nlines() == 7);
    {
        int16_t a, b, c, d;
        gfx_host_line(6, &a, &b, &c, &d);
        CHECK_EQ_INT(c, 160); CHECK_EQ_INT(d, 99);
    }
}
static void all_in_range(void) {
    int i;
    for (i = 0; i < nlines(); i++) {
        int16_t a, b, c, d;
        gfx_host_line(i, &a, &b, &c, &d);
        CHECK(a >= 0 && a <= 319 && c >= 0 && c <= 319);
        CHECK(b >= 0 && b <= 199 && d >= 0 && d <= 199);
    }
}
static void offscreen_clipped(void) {
    CHECK_EQ_STR(run("begin forward(100000) end."), "");
    CHECK_EQ_INT(nlines(), 1);
    line_is(0, 160, 99, 160, 0);
    all_in_range();
    CHECK_EQ_STR(run("begin moveto(1000000000, -1000000000) end."), "");
    CHECK_EQ_INT(nlines(), 1);
    line_is(0, 160, 99, 260, 199);          /* 45 degree diagonal down-right: ends at the bottom edge */
    all_in_range();
    CHECK_EQ_STR(run("begin moveto(1000000000, 1000000000); moveto(-1000000000, -1000000000); moveto(0, 0) end."), "");
    all_in_range();
}
static void offscreen_diagonal_exact(void) {
    /* heading 45 degrees, far away: slope must stay 1 (no distortion from int16 saturation) */
    CHECK_EQ_STR(run("begin turn(45); forward(1000000) end."), "");
    CHECK_EQ_INT(nlines(), 1);
    {
        int16_t a, b, c, d;
        gfx_host_line(0, &a, &b, &c, &d);
        CHECK_EQ_INT(a, 160); CHECK_EQ_INT(b, 99);
        CHECK_EQ_INT(d, 0);
        CHECK(c >= 258 && c <= 260);        /* 160 + 99 */
    }
    /* shallow angle: nearly horizontal, must not turn into a 45 degree line */
    CHECK_EQ_STR(run("begin turn(89); forward(1000000) end."), "");
    {
        int16_t a, b, c, d;
        gfx_host_line(0, &a, &b, &c, &d);
        CHECK_EQ_INT(c, 319);
        CHECK(d >= 96 && d <= 99);
    }
}
static void both_ends_offscreen(void) {
    /* line across the screen with both endpoints far away */
    CHECK_EQ_STR(run("begin penup; moveto(-1000000, 0); pendown; moveto(1000000, 0) end."), "");
    CHECK_EQ_INT(nlines(), 1);
    line_is(0, 0, 99, 319, 99);
    /* line missing the screen entirely */
    CHECK_EQ_STR(run("begin penup; moveto(-1000000, 5000); pendown; moveto(1000000, 5000) end."), "");
    CHECK_EQ_INT(nlines(), 0);
}
static void gfx_line_direct(void) {
    gfx_host_reset();
    gfx_line(-32768, -32768, 32767, 32767);      /* extreme corners */
    gfx_line(-32768, 100, 32767, 100);
    gfx_line(5, 5, 5, 5);
    gfx_line(400, 10, 500, 20);                  /* outside */
    gfx_line(-5, -5, 330, 210);
    CHECK_EQ_INT(nlines(), 4);
    all_in_range();
}
/* Exact clipper checks. ex* ignored when vis == 0. */
static void clipt(int line, int x0, int y0, int x1, int y1, int vis, int ex0, int ey0, int ex1, int ey1) {
    int16_t a = (int16_t)x0, b = (int16_t)y0, c = (int16_t)x1, d = (int16_t)y1;
    int r = clip_line(&a, &b, &c, &d);
    test_checks++;
    if (r != vis || (vis && (a != ex0 || b != ey0 || c != ex1 || d != ey1))) {
        test_failures++;
        printf("FAIL %s:%d: clip(%d,%d,%d,%d) -> %d (%d,%d,%d,%d), expected %d (%d,%d,%d,%d)\n",
               __FILE__, line, x0, y0, x1, y1, r, a, b, c, d, vis, ex0, ey0, ex1, ey1);
    }
}
#define CLIP(a,b,c,d, vis, e0,e1,e2,e3) clipt(__LINE__, a,b,c,d, vis, e0,e1,e2,e3)
static void clip_exact(void) {
    int16_t a, b, c, d;
    /* corners themselves, as points and as lines */
    CLIP(0,0,0,0, 1, 0,0,0,0);
    CLIP(319,199,319,199, 1, 319,199,319,199);
    CLIP(319,0,319,0, 1, 319,0,319,0);
    CLIP(0,199,0,199, 1, 0,199,0,199);
    CLIP(0,0,319,199, 1, 0,0,319,199);
    CLIP(319,0,0,199, 1, 319,0,0,199);
    /* touching a corner only: (-5,5)-(5,-5) passes through (0,0); at x=0, y=5+(-10)*5/10=0; at y=0, x=-5+10*5/10=0 */
    CLIP(-5,5,5,-5, 1, 0,0,0,0);
    CLIP(314,204,324,194, 1, 319,199,319,199);
    CLIP(324,5,314,-5, 1, 319,0,319,0);
    CLIP(-5,194,5,204, 1, 0,199,0,199);
    /* just missing the corner: (-5,3)-(3,-5) crosses x=0 at y=-2 (above the screen) */
    CLIP(-5,3,3,-5, 0, 0,0,0,0);
    CLIP(324,196,316,204, 0, 0,0,0,0);
    /* zero-length lines */
    CLIP(0,50,0,50, 1, 0,50,0,50);
    CLIP(319,50,319,50, 1, 319,50,319,50);
    CLIP(160,0,160,0, 1, 160,0,160,0);
    CLIP(160,99,160,99, 1, 160,99,160,99);
    CLIP(-1,50,-1,50, 0, 0,0,0,0);
    CLIP(320,50,320,50, 0, 0,0,0,0);
    CLIP(10,200,10,200, 0, 0,0,0,0);
    /* horizontal and vertical lines along the edges */
    CLIP(0,0,319,0, 1, 0,0,319,0);
    CLIP(0,199,319,199, 1, 0,199,319,199);
    CLIP(0,0,0,199, 1, 0,0,0,199);
    CLIP(319,0,319,199, 1, 319,0,319,199);
    /* spanning the full int16 range along the edges (dx = 65535) */
    CLIP(-32768,0,32767,0, 1, 0,0,319,0);
    CLIP(-32768,199,32767,199, 1, 0,199,319,199);
    CLIP(0,-32768,0,32767, 1, 0,0,0,199);
    CLIP(319,-32768,319,32767, 1, 319,0,319,199);
    CLIP(32767,0,-32768,0, 1, 319,0,0,0);
    CLIP(160,32767,160,-32768, 1, 160,199,160,0);
    /* entirely off one side, at both int16 extremes */
    CLIP(-32768,-32768,-32768,32767, 0, 0,0,0,0);
    CLIP(32767,-32768,32767,32767, 0, 0,0,0,0);
    CLIP(-32768,-32768,32767,-32768, 0, 0,0,0,0);
    CLIP(-32768,32767,32767,32767, 0, 0,0,0,0);
    CLIP(-32768,-1,32767,-1, 0, 0,0,0,0);
    CLIP(-32768,200,32767,200, 0, 0,0,0,0);
    CLIP(-1,-32768,-1,32767, 0, 0,0,0,0);
    CLIP(320,-32768,320,32767, 0, 0,0,0,0);
    /* diagonal y=x from far outside both ends: enters at (0,0) (top: y=0, x=-1000+2000*1000/2000=0),
       leaves at the bottom y=199 (x=-1000+2000*1199/2000=199) */
    CLIP(-1000,-1000,1000,1000, 1, 0,0,199,199);
    /* extreme corners: y=x with dx=dy=65535 gives the same line */
    CLIP(-32768,-32768,32767,32767, 1, 0,0,199,199);
    /* y=x/2 from (-1000,-500) to (1000,500): at x=0 y=-500+1000*1000/2000=0;
       at x=319 y=-500+1000*1319/2000=159.5, rounded to nearest (half up) = 160 */
    CLIP(-1000,-500,1000,500, 1, 0,0,319,160);
    /* opposite direction: same line, rounding at the other end may differ by one */
    a = 1000; b = 500; c = -1000; d = -500;
    CHECK_EQ_INT(clip_line(&a, &b, &c, &d), 1);
    CHECK_EQ_INT(a, 319); CHECK(b == 159 || b == 160); CHECK_EQ_INT(c, 0); CHECK_EQ_INT(d, 0);
    /* inside to far outside */
    CLIP(160,99,160,-32768, 1, 160,99,160,0);
    CLIP(160,99,160,32767, 1, 160,99,160,199);
    CLIP(160,99,-32768,99, 1, 160,99,0,99);
    CLIP(160,99,32767,99, 1, 160,99,319,99);
    /* inside to (32767,32767): hits y=199 first, x=160+32607*100/32668=259.8 -> 260 */
    CLIP(160,99,32767,32767, 1, 160,99,260,199);
    /* the same through gfx_line records only the clipped line */
    gfx_host_reset();
    gfx_line(-1000, -1000, 1000, 1000);
    CHECK_EQ_INT(nlines(), 1);
    line_is(0, 0, 0, 199, 199);
    gfx_line(-32768, -32768, -32768, 32767);
    gfx_line(32767, -32768, 32767, 32767);
    CHECK_EQ_INT(nlines(), 1);
}
static void huge_turn(void) {
    CHECK_EQ_STR(run("begin turn(10000000); forward(10) end."), "");
    CHECK_EQ_INT(nlines(), 1);
    /* 1e7 mod 360 = 280: heading 280 = pointing left and slightly up */
    {
        int16_t a, b, c, d;
        gfx_host_line(0, &a, &b, &c, &d);
        CHECK_EQ_INT(c, 150);
        CHECK(d == 97 || d == 98);
    }
    CHECK_EQ_STR(run("begin turn(1000000000*1000000000*1000000000*1000000000); forward(10) end."), "");
    all_in_range();
    CHECK_EQ_STR(run("begin turn(-10000000); turn(10000000); forward(10) end."), "");
    line_is(0, 160, 99, 160, 89);
}
static void pbm_written(void) {
    FILE *f;
    char buf[100];
    long ones = 0;
    int ch;
    CHECK_EQ_STR(run("var i; begin i := 0; while i < 4 do begin forward(50); turn(90); i := i + 1 end end."), "");
    CHECK_EQ_INT(gfx_host_write_pbm("build/test_turtle.pbm"), 0);
    f = fopen("build/test_turtle.pbm", "rb");
    CHECK(f != NULL);
    if (!f) return;
    CHECK(fgets(buf, sizeof buf, f) != NULL);
    CHECK_EQ_STR(buf, "P1\n");
    CHECK(fgets(buf, sizeof buf, f) != NULL);
    CHECK_EQ_STR(buf, "320 200\n");
    while ((ch = fgetc(f)) != EOF) if (ch == '1') ones++;
    fclose(f);
    CHECK_EQ_INT(ones, 200);               /* square outline: 4 x 50 unique pixels */
}
static void lines_dump(void) {
    FILE *f;
    char buf[100];
    CHECK_EQ_STR(run("begin forward(10); turn(90); forward(5) end."), "");
    CHECK_EQ_INT(gfx_host_write_lines("build/test_turtle.lines"), 0);
    f = fopen("build/test_turtle.lines", "rb");
    CHECK(f != NULL);
    if (!f) return;
    CHECK(fgets(buf, sizeof buf, f) != NULL);
    CHECK_EQ_STR(buf, "160 99 160 89\n");
    CHECK(fgets(buf, sizeof buf, f) != NULL);
    CHECK_EQ_STR(buf, "160 89 165 89\n");
    fclose(f);
}
static void syntax_errors(void) {
    CHECK_EQ_STR(run("forward 5."), "ERROR line 1: \"(\" expected");
    CHECK_EQ_STR(run("forward(5."), "ERROR line 1: \")\" expected");
    CHECK_EQ_STR(run("turn."), "ERROR line 1: \"(\" expected");
    CHECK_EQ_STR(run("forward()."), "ERROR line 1: expression expected");
    CHECK_EQ_STR(run("moveto(1)."), "ERROR line 1: \")\" expected");
    CHECK_EQ_STR(run("moveto(1,2."), "ERROR line 1: \")\" expected");
    CHECK_EQ_STR(run("moveto 1,2."), "ERROR line 1: \"(\" expected");
    CHECK_EQ_STR(run("begin penup(1) end."), "ERROR line 1: \"end\" expected");
    CHECK_EQ_STR(run("begin penup pendown end."), "ERROR line 1: \";\" expected");
    CHECK_EQ_STR(run("begin home; forward(1) home end."), "ERROR line 1: \";\" expected");
}
static void runtime_overflow(void) {
    /* the multiplication itself overflows */
    CHECK_EQ_STR(run("var a; begin a := 1000000000; a := a*a; a := a*a; a := a*a; a := a*a; a := a*a; a := a*a; a := a*a; forward(a) end."),
                 "ERROR line 1: number overflow");
    /* the turtle's own arithmetic overflows: 1e308 + 1e308 */
    CHECK_EQ_STR(run("var a, i; begin a := 1000000; a := a*a; a := a*a; a := a*a; a := a*a; a := a*a; i := 0;\n"
                     "while i < 12 do begin a := a*1000000000; i := i+1 end;\n"
                     "a := a*100000000;\n"
                     "forward(a);\n"
                     "forward(a)\n"
                     "end."),
                 "ERROR line 5: number overflow");
    all_in_range();
    CHECK_EQ_STR(run("var a, i; begin a := 1000000; a := a*a; a := a*a; a := a*a; a := a*a; a := a*a; i := 0;\n"
                     "while i < 12 do begin a := a*1000000000; i := i+1 end;\n"
                     "a := a*100000000;\n"
                     "forward(a); forward(-a); moveto(a, a)\n"
                     "end."),
                 "");                      /* huge but finite: clipped, no error */
    all_in_range();
}
static void args_use_variables(void) {
    CHECK_EQ_STR(run("var a, b; begin a := 3; b := 4; moveto(a*b, -a-b) end."), "");
    line_is(0, 160, 99, 172, 106);
}
static void trig_cache(void) {
    /* counter counts sin and cos evaluations; heading 0 (start, home) uses preset values */
    CHECK_EQ_STR(run("var i; begin i := 0; while i < 1000 do begin forward(0.01); i := i + 1 end end."), "");
    CHECK_EQ_INT(nlines(), 1000);
    CHECK_EQ_INT(turtle_trig_count(), 0);
    CHECK_EQ_STR(run("var i; begin turn(10); i := 0; while i < 1000 do begin forward(0.01); i := i + 1 end end."), "");
    CHECK_EQ_INT(nlines(), 1000);
    CHECK_EQ_INT(turtle_trig_count(), 2);          /* one sin + one cos for the single turn */
    CHECK_EQ_STR(run("begin turn(10); turn(20); forward(1); forward(1) end."), "");
    CHECK_EQ_INT(turtle_trig_count(), 4);
    CHECK_EQ_STR(run("begin turn(0); forward(1); forward(1); home; forward(1) end."), "");
    CHECK_EQ_INT(turtle_trig_count(), 0);          /* turn(0) and home do not compute */
}
/* "<head>(" + n parens around 1 + ")" + tail, using the 16-level expression limit */
static const char *nested(const char *head, int n, const char *tail) {
    static char buf[200];
    int i;
    strcpy(buf, head);
    for (i = 0; i < n; i++) strcat(buf, "(");
    strcat(buf, "1");
    for (i = 0; i < n; i++) strcat(buf, ")");
    strcat(buf, tail);
    return buf;
}
static void nesting_counters(void) {
    /* the statement's own argument list takes 1 of the 16 levels */
    CHECK_EQ_STR(run(nested("begin forward(", 15, ") end.")), "");
    CHECK_EQ_STR(run(nested("begin forward(", 16, ") end.")), "ERROR line 1: nested too deeply");
    CHECK_EQ_STR(run(nested("begin turn(", 16, ") end.")), "ERROR line 1: nested too deeply");
    CHECK_EQ_STR(run(nested("begin moveto(", 15, ", 1) end.")), "");
    CHECK_EQ_STR(run(nested("begin moveto(1, ", 15, ") end.")), "");
    CHECK_EQ_STR(run(nested("begin moveto(1, ", 16, ") end.")), "ERROR line 1: nested too deeply");
}
static void stack_depth_ok(void) {
    /* temporaries inside moveto args are accounted for: deep expression in both args */
    CHECK_EQ_STR(run("begin moveto(1+(2+(3+(4+5))), 1+(2+(3+(4+5)))) end."), "");
    line_is(0, 160, 99, 175, 84);
}

int main(void) {
    forward_up(); no_turtle_not_used(); turn_right(); turn_negative(); penup_no_line();
    home_resets_position_and_heading(); moveto_draws_when_pen_down(); square();
    polygon_closes(); offscreen_clipped(); offscreen_diagonal_exact();
    both_ends_offscreen(); gfx_line_direct(); clip_exact(); huge_turn(); pbm_written();
    lines_dump(); syntax_errors(); runtime_overflow(); args_use_variables();
    trig_cache(); nesting_counters(); stack_depth_ok();
    TEST_MAIN_END();
}
