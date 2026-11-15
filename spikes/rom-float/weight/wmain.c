/* wmain.c -- SPIKE weight stub: arrays at draft limits, platform stubs,
 * and a VM speed benchmark. run.sh breaks at spike_done and dumps logbuf. */
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "weight.h"

Ident idents[MAX_IDENTS];
#ifdef SOURCE_IN_BASICBSS
#pragma bss-name (push, "BASICBSS")    /* $A000-$BFFF: RAM under BASIC ROM */
#endif
char  source[MAX_SOURCE];
#ifdef SOURCE_IN_BASICBSS
#pragma bss-name (pop)
#endif
char  logbuf[1024];
static unsigned logpos;

static void say(const char *s)
{
    unsigned n = strlen(s);
    puts(s);
    if (logpos + n + 2 < sizeof logbuf) {
        memcpy(logbuf + logpos, s, n); logpos += n;
        logbuf[logpos++] = '\n'; logbuf[logpos] = 0;
    }
}
void spike_done(void) { for (;;) ; }

/* platform stubs (real ones come in Tasks 9, 12) */
void io_put_str(const char *s) { fputs(s, stdout); }
void io_put_nl(void) { putchar('\n'); }
int  io_read_num(num_t *r) { (void)r; return 1; }
int  io_break(void) { return 0; }
void turtle_forward(const num_t *d) { (void)d; }
void turtle_turn(const num_t *d) { (void)d; }
void turtle_pen(uint8_t down) { (void)down; }
void turtle_home(void) { }
void turtle_moveto(const num_t *x, const num_t *y) { (void)x; (void)y; }

#ifdef VM2
#define SLOT(n) ((n) * 5)          /* vm2: operands are byte offsets */
#define NVARS(n) ((n) + 2)         /* vm2: slots 1.. are variables */
#else
#define SLOT(n) (n)
#define NVARS(n) (n)
#endif
static uint16_t n_code;
static void emit(uint8_t op, uint8_t lev, uint16_t a)
{
    code[n_code].op = op; code[n_code].lev = lev; code[n_code].a = a; ++n_code;
}

/* time `reps` runs of the program in code[]; returns jiffies */
static unsigned bench(unsigned reps)
{
    clock_t t0 = clock();
    while (reps--) vm_run();
    return (unsigned)(clock() - t0);
}

static char line[80];
static void report(const char *what, unsigned jif, unsigned long ninstr)
{
    /* us per instruction = jif/60 s / ninstr */
    sprintf(line, "%s: %u jif, %lu instr, %lu us/instr", what, jif, ninstr,
            (unsigned long)jif * 1000000UL / 60UL / ninstr);
    say(line);
}

int main(void)
{
    unsigned j, i;
    fp_instrument = 0;
    /* RAM under BASIC ROM: fill source[] with a pattern, do ROM float work
     * (which banks BASIC in), check the pattern and the result survive. */
    {
        num_t a, b, r; char t[16]; unsigned bad = 0;
        for (i = 0; i < MAX_SOURCE; ++i) source[i] = (char)(i * 7 + 3);
        num_from_int(&a, 1); num_from_int(&b, 3);
        for (i = 0; i < 50; ++i) num_div(&r, &a, &b);
        num_format(&r, t);
        for (i = 0; i < MAX_SOURCE; ++i) if (source[i] != (char)(i * 7 + 3)) ++bad;
        sprintf(line, "source@%04x..%04x bad=%u 1/3=%s $01=%02x",
                (unsigned)source, (unsigned)(source + MAX_SOURCE - 1), bad, t,
                *(unsigned char *)1);
        say(line);
        sprintf(line, "code@%04x stack@%04x idents@%04x", (unsigned)code,
                (unsigned)stack, (unsigned)idents);
        say(line);
    }
    num_from_int(&consts[0], 0);
    num_from_int(&consts[1], 1);
    num_from_int(&consts[2], 500);

    /* A: 700 x (LOD, STO) straight line = pure dispatch + 5-byte copies */
    n_code = 0;
    emit(OP_INT, 0, NVARS(2));
    for (i = 0; i < 350 && n_code < MAX_CODE - 2; ++i) { emit(OP_LOD, 0, SLOT(3)); emit(OP_STO, 0, SLOT(4)); }
    emit(OP_HLT, 0, 0);
    j = bench(20);
    report("lod/sto  ", j, 20UL * n_code);
#ifdef VMASM
    {
        extern void vmasm_run(void);
        clock_t t0 = clock();
        for (i = 0; i < 20; ++i) vmasm_run();
        report("asm lod/sto", (unsigned)(clock() - t0), 20UL * n_code);
    }
#endif

    /* B: i := 0; while i < 500 do i := i + 1   (9 instructions per iteration) */
    n_code = 0;
    emit(OP_INT, 0, NVARS(1));                          /* var i at stack[3] */
    emit(OP_LIT, 0, 0); emit(OP_STO, 0, SLOT(3));
    emit(OP_LOD, 0, SLOT(3)); emit(OP_LIT, 0, 2); emit(OP_LT, 0, 0); emit(OP_JPC, 0, 12);
    emit(OP_LOD, 0, SLOT(3)); emit(OP_LIT, 0, 1); emit(OP_ADD, 0, 0); emit(OP_STO, 0, SLOT(3));
    emit(OP_JMP, 0, 3);
    emit(OP_HLT, 0, 0);
    j = bench(1);
    report("while+add", j, 3UL + 500UL * 9UL + 5UL);

    /* C: same loop, body i := i * 1 + 1  (11 instructions per iteration) */
    n_code = 0;
    emit(OP_INT, 0, NVARS(1));
    emit(OP_LIT, 0, 0); emit(OP_STO, 0, SLOT(3));
    emit(OP_LOD, 0, SLOT(3)); emit(OP_LIT, 0, 2); emit(OP_LT, 0, 0); emit(OP_JPC, 0, 14);
    emit(OP_LOD, 0, SLOT(3)); emit(OP_LIT, 0, 1); emit(OP_MUL, 0, 0);
    emit(OP_LIT, 0, 1); emit(OP_ADD, 0, 0); emit(OP_STO, 0, SLOT(3));
    emit(OP_JMP, 0, 3);
    emit(OP_HLT, 0, 0);
    j = bench(1);
    report("while+mul", j, 3UL + 500UL * 11UL + 5UL);

    if (editor_run == 0) say("never");   /* keep the editor linked */
    say("done");
    spike_done();
    return 0;
}
