/* P-code interpreter.
 *
 * Written in the cc65-friendly style the Task 1 spike measured (section 8 of
 * docs/architecture/spike-results.md): num_t pointers for the stack, byte
 * offsets pre-scaled by the compiler (no multiply by 5 per access), a display
 * disp[] indexed by absolute declaration level instead of a static-link walk,
 * num_copy instead of struct assignment, state in statics.
 *
 * Stack: sp points to the next free cell, bp to the current frame base.
 * The stack is checked only when a frame is created: CAL checks room for the
 * whole new frame (header, locals and the block's maximum number of
 * temporaries, read from the callee's INT) before writing anything, and INT
 * checks again before reserving (this covers the main program). So pushes
 * need no check and the stack top never passes MAX_STACK. A runaway
 * recursion ends with E_STACK at the line of the call that did not fit.
 *
 * Frame header cells hold raw pointers (memcpy, no num_t semantics). */
#include <string.h>
#include "core/vm.h"
#include "core/turtle.h"
#include "platform/io.h"

#define CELL_AT(base, off) ((num_t *)((char *)(base) + (off)))

/* On the C64 the stack (2,560 bytes) lives in HIBSS ($C000-$CBFF, see
 * src/c64/pl0-c64.cfg and docs/architecture/memory-map.md): low RAM is
 * full. It needs no clearing (every cell is written before it is read). */
#ifdef __CC65__
#pragma bss-name (push, "HIBSS")
#endif
static num_t stack[MAX_STACK];
#ifdef __CC65__
#pragma bss-name (pop)
#endif
static num_t *disp[MAX_NEST + 1];   /* frame base per declaration level */
static char numbuf[16];
#ifdef PL0_TEST_HOOKS
static num_t *high;                 /* highest sp seen (test hook) */
static unsigned long icount;        /* instructions executed (test hook) */
static unsigned long fncount;       /* FN instructions executed (test hook) */
#endif

/* A header cell must be able to hold a pointer (on the C64: 5 >= 2). */
#ifndef __CC65__
typedef char vm_cell_holds_pointer[sizeof(num_t) >= sizeof(void *) ? 1 : -1];
#endif

/* Nonzero if fewer than need bytes are free above sp. */
static int no_room(const num_t *sp, uint16_t need)
{
    return (size_t)((const char *)(stack + MAX_STACK) - (const char *)sp) < (size_t)need;
}

/* Bytes a block's frame needs above its header: locals plus temporaries. */
static uint16_t frame_need(const Instr *in)
{
    return (uint16_t)(FRAME_BYTES(in->arg) + (uint16_t)in->lvl * sizeof(num_t));
}

static int fail(PL0Error *err, uint8_t code, int16_t line)
{
    err->code = code;
    err->line = (uint16_t)line;
    return 1;
}

int pl0_run(const Program *p, PL0Error *err)
{
    const Instr *ip = p->code;
    const Instr *ra;
    num_t *sp = stack + FRAME_HDR;      /* main frame header: unused */
    num_t *bp = stack;
    uint8_t l;
    int c;

    err->code = E_NONE;
    err->line = 0;
    disp[0] = stack;
    turtle_reset();
#ifdef PL0_TEST_HOOKS
    high = sp;
    icount = 0;
    fncount = 0;
#endif
    for (;;) {
#ifdef PL0_TEST_HOOKS
        if (sp > high) high = sp;
        ++icount;
#endif
        switch (ip->op) {
        case OP_LIT:
            num_copy(sp, (const num_t *)((const char *)p->consts + ip->arg));
            ++sp;
            break;
        case OP_LOD:
            num_copy(sp, CELL_AT(disp[ip->lvl], ip->arg));
            ++sp;
            break;
        case OP_STO:
            --sp;
            num_copy(CELL_AT(disp[ip->lvl], ip->arg), sp);
            break;
        case OP_CAL:
            /* The caller has pushed the arguments. Check room for the whole
             * new frame before writing its header. */
            ra = p->code + CAL_ADDR(ip->arg);           /* callee's INT */
            if (no_room(sp, (uint16_t)(FRAME_HDR * sizeof(num_t) + frame_need(ra))))
                return fail(err, E_STACK, (int16_t)CAL_LINE(ip));
            memcpy(sp + FRAME_DL, &bp, sizeof bp);
            ++ip;
            memcpy(sp + FRAME_RA, &ip, sizeof ip);
            bp = sp;
            sp += FRAME_HDR;
            ip = ra;
            continue;
        case OP_INT:
            if (no_room(sp, frame_need(ip))) {
                /* Only reachable for the main program (CAL checked a
                 * procedure's frame); report the call's line if any. */
                if (bp == stack) return fail(err, E_STACK, 1);
                memcpy(&ra, bp + FRAME_RA, sizeof ra);
                return fail(err, E_STACK, (int16_t)CAL_LINE(ra - 1));
            }
            l = FRAME_LEVEL(ip->arg);
            memcpy(bp + FRAME_SL, &disp[l], sizeof disp[l]);
            disp[l] = bp;
            memset(sp, 0, FRAME_BYTES(ip->arg));   /* variables start at 0 */
            sp = CELL_AT(sp, FRAME_BYTES(ip->arg));
            break;
        case OP_OPR:
            switch (ip->lvl) {
            case OPR_RET:
                if (bp == stack) return 0;       /* main program ends */
                l = FRAME_LEVEL(ip->arg);
                memcpy(&disp[l], bp + FRAME_SL, sizeof disp[l]);
                sp = CELL_AT(bp, -(int16_t)FRAME_BYTES(ip->arg));  /* pop params */
                memcpy(&ip, bp + FRAME_RA, sizeof ip);
                memcpy(&bp, bp + FRAME_DL, sizeof bp);
                continue;
            case OPR_NEG:
                if (num_neg(sp - 1, sp - 1)) return fail(err, E_OVERFLOW, ip->arg);
                break;
            case OPR_ADD:
                --sp;
                if (num_add(sp - 1, sp - 1, sp)) return fail(err, E_OVERFLOW, ip->arg);
                break;
            case OPR_SUB:
                --sp;
                if (num_sub(sp - 1, sp - 1, sp)) return fail(err, E_OVERFLOW, ip->arg);
                break;
            case OPR_MUL:
                --sp;
                if (num_mul(sp - 1, sp - 1, sp)) return fail(err, E_OVERFLOW, ip->arg);
                break;
            case OPR_ODD:
                num_from_int(sp - 1, (int16_t)num_is_odd(sp - 1));
                break;
            case OPR_EQ: case OPR_NE: case OPR_LT:
            case OPR_GE: case OPR_GT: case OPR_LE:
                --sp;
                c = num_cmp(sp - 1, sp);
                switch (ip->lvl) {
                case OPR_EQ: c = c == 0; break;
                case OPR_NE: c = c != 0; break;
                case OPR_LT: c = c < 0; break;
                case OPR_GE: c = c >= 0; break;
                case OPR_GT: c = c > 0; break;
                default:     c = c <= 0; break;
                }
                num_from_int(sp - 1, c);
                break;
            case OPR_DIV:
                --sp;
                c = num_div(sp - 1, sp - 1, sp);
                if (c) return fail(err, c == 1 ? E_DIV_ZERO : E_OVERFLOW, ip->arg);
                break;
            }
            break;
        case OP_JMP:
            /* Only the while loop-back jumps backward. */
            if (ip->arg <= (int16_t)(ip - p->code) && io_break()) {
                /* Line of the loop: from the loop condition, whose last
                 * instruction (just before its JPC) is an OPR with the line. */
                const Instr *q = p->code + ip->arg;
                while (q->op != OP_JPC) ++q;
                return fail(err, E_STOPPED, (q - 1)->arg);
            }
            ip = p->code + ip->arg;
            continue;
        case OP_JPC:
            --sp;
            if (num_is_zero(sp)) {
                ip = p->code + ip->arg;
                continue;
            }
            break;
        case OP_FN:
#ifdef PL0_TEST_HOOKS
            ++fncount;
#endif
            c = num_fn(ip->lvl, sp - 1, sp - 1);
            if (c) return fail(err, c == 1 ? E_DOMAIN : E_OVERFLOW, ip->arg);
            break;
        case OP_TUR:
            switch (ip->lvl) {
            case TUR_FORWARD:
                --sp;
                c = turtle_forward(sp);
                break;
            case TUR_TURN:
                --sp;
                c = turtle_turn(sp);
                break;
            case TUR_MOVETO:
                sp -= 2;
                c = turtle_moveto(sp, sp + 1);
                break;
            case TUR_PENUP:   turtle_penup(); c = 0; break;
            case TUR_PENDOWN: turtle_pendown(); c = 0; break;
            default:          turtle_home(); c = 0; break;
            }
            if (c) return fail(err, E_OVERFLOW, ip->arg);
            break;
        case OP_RED:
            if (io_read_num(sp)) return fail(err, E_BAD_INPUT, ip->arg);
            ++sp;
            break;
        case OP_WRN:
            --sp;
            num_format(sp, numbuf);
            io_put_str(numbuf);
            break;
        case OP_WRS:
            io_put_str(p->strings + ip->arg);
            break;
        case OP_WRL:
            io_put_nl();
            break;
        }
        ++ip;
    }
}

#ifdef PL0_TEST_HOOKS
unsigned vm_stack_high_water(void) { return (unsigned)(high - stack); }
unsigned long vm_instr_count(void) { return icount; }
unsigned long vm_fn_count(void) { return fncount; }
#endif
