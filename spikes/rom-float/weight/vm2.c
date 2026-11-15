/* vm2.c -- SPIKE: the same P-code VM written "cc65-friendly":
 *   - stack and frames addressed with pointers, never index * 5,
 *   - LOD/STO operands pre-scaled to byte offsets by the compiler,
 *   - a display (MAX_NEST + 1 frame pointers) instead of walking static
 *     links; instructions carry the absolute declaration level,
 *   - 5-byte copies through a tiny fastcall-free helper on static pointers.
 * Still plain portable C. Compare with vm.c in docs/architecture/spike-results.md. */
#include <string.h>
#include "weight.h"

#ifdef CODE_IN_HIBSS
#pragma bss-name (push, "HIBSS")
#endif
Instr code[MAX_CODE];
#ifdef CODE_IN_HIBSS
#pragma bss-name (pop)
#endif
num_t stack[MAX_STACK];
num_t consts[MAX_CONSTS];
char  strings[MAX_STRINGS];

enum { E_OK, E_DIV_ZERO, E_DOMAIN, E_STACK, E_STOPPED, E_BAD_INPUT, E_BAD_OP };

static num_t *tp;                         /* top element */
static num_t *disp[MAX_NEST + 1];         /* frame base per static level */
unsigned char *s, *d;                     /* copy helper operands */
#define STACK_END (&stack[MAX_STACK - 4])

#ifdef ASMCOPY
void copy5(void);                         /* copy5.s: 5-byte copy *s -> *d */
#else
static void copy5(void)
{
    register unsigned char *dd = d, *ss = s;   /* zero page with -Or */
    dd[0] = ss[0]; dd[1] = ss[1]; dd[2] = ss[2]; dd[3] = ss[3]; dd[4] = ss[4];
}
#endif

/* frame: [0] saved disp[level] (pointer), [1] caller frame top, [2] return pc */
typedef struct { num_t *saved; num_t *ret_tp; Instr *ret_ip; uint8_t lev; } Link;

uint8_t vm_run(void)
{
    Instr *ip = code;
    num_t *f;
    Link *lk;
    int c;
    static char buf[20];

    tp = stack;
    disp[0] = stack;
    for (;;) {
        switch (ip->op) {
        case OP_LOD:
            if (tp >= STACK_END) return E_STACK;
            s = (unsigned char *)disp[ip->lev] + ip->a;
            d = (unsigned char *)++tp;
            copy5();
            break;
        case OP_STO:
            s = (unsigned char *)tp--;
            d = (unsigned char *)disp[ip->lev] + ip->a;
            copy5();
            break;
        case OP_LIT:
            if (tp >= STACK_END) return E_STACK;
            s = (unsigned char *)&consts[ip->a];
            d = (unsigned char *)++tp;
            copy5();
            break;
        case OP_JMP:
            ip = code + ip->a;
            if (io_break()) return E_STOPPED;
            continue;
        case OP_JPC:
            if (!tp->b[0]) { ip = code + ip->a; --tp; continue; }
            --tp;
            break;
        case OP_CAL:                       /* lev = callee's level */
            if (tp >= STACK_END) return E_STACK;
            f = tp + 1;
            lk = (Link *)f;                /* Link is 7 bytes, fits in 2 slots */
            lk->saved = disp[ip->lev];
            lk->ret_tp = tp;
            lk->ret_ip = ip + 1;
            lk->lev = ip->lev;
            disp[ip->lev] = f;
            tp = f + 1;                    /* locals follow (INT reserves) */
            ip = code + ip->a;
            if (io_break()) return E_STOPPED;
            continue;
        case OP_RET:
            f = disp[ip->lev];
            if (f == stack) return E_OK;
            lk = (Link *)f;
            disp[ip->lev] = lk->saved;
            tp = lk->ret_tp - ip->a;       /* drop parameters */
            ip = lk->ret_ip;
            continue;
        case OP_INT:
            tp += ip->a;
            if (tp >= STACK_END) return E_STACK;
            break;
        case OP_NEG:
            num_neg(tp, tp);
            break;
        case OP_ADD: --tp; num_add(tp, tp, tp + 1); break;
        case OP_SUB: --tp; num_sub(tp, tp, tp + 1); break;
        case OP_MUL: --tp; num_mul(tp, tp, tp + 1); break;
        case OP_DIV:
            --tp;
            if (num_div(tp, tp, tp + 1)) return E_DIV_ZERO;
            break;
        case OP_ODD:
            c = num_is_odd(tp);
            goto setbool;
        case OP_EQ: case OP_NE: case OP_LT: case OP_LE: case OP_GT: case OP_GE:
            --tp;
            c = num_cmp(tp, tp + 1);
            switch (ip->op) {
            case OP_EQ: c = (c == 0); break;
            case OP_NE: c = (c != 0); break;
            case OP_LT: c = (c < 0); break;
            case OP_LE: c = (c <= 0); break;
            case OP_GT: c = (c > 0); break;
            default:    c = (c >= 0); break;
            }
        setbool:
            if (c) { s = (unsigned char *)&consts[1]; d = (unsigned char *)tp; copy5(); }
            else tp->b[0] = 0;             /* exponent 0 == zero */
            break;
        case OP_FN:
            if (num_fn(ip->a, tp, tp)) return E_DOMAIN;
            break;
        case OP_WRT:
            num_format(tp--, buf);
            io_put_str(buf);
            break;
        case OP_WRS:
            io_put_str(strings + ip->a);
            break;
        case OP_WRL:
            io_put_nl();
            break;
        case OP_RD:
            if (io_read_num((num_t *)((unsigned char *)disp[ip->lev] + ip->a))) return E_BAD_INPUT;
            break;
        case OP_FWD: turtle_forward(tp--); break;
        case OP_TRN: turtle_turn(tp--); break;
        case OP_PU: turtle_pen(0); break;
        case OP_PD: turtle_pen(1); break;
        case OP_HOME: turtle_home(); break;
        case OP_MOVETO:
            tp -= 2;
            turtle_moveto(tp + 1, tp + 2);
            break;
        case OP_HLT:
            return E_OK;
        default:
            return E_BAD_OP;
        }
        ++ip;
    }
}
