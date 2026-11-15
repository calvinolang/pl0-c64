/* vm.c -- SPIKE weight stub: a realistic switch-based P-code interpreter
 * loop (target ~2 KB of 6502 code). Not reused; measures size and speed. */
#include <string.h>
#include "weight.h"

#ifdef CODE_IN_HIBSS
#pragma bss-name (push, "HIBSS")       /* $C000-$CBFF, see pl0-c64.cfg */
#endif
Instr code[MAX_CODE];
#ifdef CODE_IN_HIBSS
#pragma bss-name (pop)
#endif
num_t stack[MAX_STACK];
num_t consts[MAX_CONSTS];
char  strings[MAX_STRINGS];

enum { E_OK, E_DIV_ZERO, E_DOMAIN, E_STACK, E_STOPPED, E_BAD_INPUT, E_BAD_OP };

static uint16_t pc, bp, sp;   /* sp = index of top element */
static num_t one_tmp;

/* frame layout: [bp] static link (as number index), [bp+1] dynamic link,
 * [bp+2] return address; links are kept as 16-bit values in b[0..1]. */
static uint16_t base(uint8_t lev)
{
    uint16_t b = bp;
    while (lev--) b = stack[b].b[0] | (stack[b].b[1] << 8);
    return b;
}

static void put16(num_t *n, uint16_t v) { n->b[0] = v & 0xff; n->b[1] = v >> 8; }
static uint16_t get16(const num_t *n) { return n->b[0] | (n->b[1] << 8); }

static void set_bool(num_t *r, int c)
{
    if (c) num_from_int(r, 1); else memset(r, 0, sizeof *r);
}

uint8_t vm_run(void)
{
    Instr *ip;
    num_t *t;
    uint16_t nb;
    int c;
    static char buf[20];

    pc = 0; bp = 0; sp = 2;
    memset(stack, 0, 3 * sizeof(num_t));
    for (;;) {
        ip = &code[pc++];
        switch (ip->op) {
        case OP_LIT:
            if (sp >= MAX_STACK - 1) return E_STACK;
            stack[++sp] = consts[ip->a];
            break;
        case OP_LOD:
            if (sp >= MAX_STACK - 1) return E_STACK;
            stack[sp + 1] = stack[base(ip->lev) + ip->a];
            ++sp;
            break;
        case OP_STO:
            stack[base(ip->lev) + ip->a] = stack[sp--];
            break;
        case OP_CAL:
            if (sp + 3 >= MAX_STACK) return E_STACK;
            nb = sp + 1;
            put16(&stack[nb], base(ip->lev));
            put16(&stack[nb + 1], bp);
            put16(&stack[nb + 2], pc);
            bp = nb;
            pc = ip->a;
            if (io_break()) return E_STOPPED;
            break;
        case OP_INT:
            if (sp + ip->a >= MAX_STACK) return E_STACK;
            sp += ip->a;
            break;
        case OP_JMP:
            pc = ip->a;
            if (io_break()) return E_STOPPED;
            break;
        case OP_JPC:
            if (num_is_zero(&stack[sp])) pc = ip->a;
            --sp;
            break;
        case OP_RET:
            if (bp == 0) return E_OK;
            sp = bp - 1 - ip->a;           /* drop frame and parameters */
            pc = get16(&stack[bp + 2]);
            bp = get16(&stack[bp + 1]);
            break;
        case OP_NEG:
            num_neg(&stack[sp], &stack[sp]);
            break;
        case OP_ADD:
            --sp; num_add(&stack[sp], &stack[sp], &stack[sp + 1]);
            break;
        case OP_SUB:
            --sp; num_sub(&stack[sp], &stack[sp], &stack[sp + 1]);
            break;
        case OP_MUL:
            --sp; num_mul(&stack[sp], &stack[sp], &stack[sp + 1]);
            break;
        case OP_DIV:
            --sp;
            if (num_div(&stack[sp], &stack[sp], &stack[sp + 1])) return E_DIV_ZERO;
            break;
        case OP_ODD:
            set_bool(&stack[sp], num_is_odd(&stack[sp]));
            break;
        case OP_EQ: case OP_NE: case OP_LT: case OP_LE: case OP_GT: case OP_GE:
            --sp;
            c = num_cmp(&stack[sp], &stack[sp + 1]);
            switch (ip->op) {
            case OP_EQ: c = (c == 0); break;
            case OP_NE: c = (c != 0); break;
            case OP_LT: c = (c < 0); break;
            case OP_LE: c = (c <= 0); break;
            case OP_GT: c = (c > 0); break;
            default:    c = (c >= 0); break;
            }
            set_bool(&stack[sp], c);
            break;
        case OP_FN:
            if (num_fn(ip->a, &stack[sp], &stack[sp])) return E_DOMAIN;
            break;
        case OP_WRT:
            num_format(&stack[sp--], buf);
            io_put_str(buf);
            break;
        case OP_WRS:
            io_put_str(strings + ip->a);
            break;
        case OP_WRL:
            io_put_nl();
            break;
        case OP_RD:
            t = &stack[base(ip->lev) + ip->a];
            if (io_read_num(t)) return E_BAD_INPUT;
            break;
        case OP_FWD:
            turtle_forward(&stack[sp--]);
            break;
        case OP_TRN:
            turtle_turn(&stack[sp--]);
            break;
        case OP_PU: turtle_pen(0); break;
        case OP_PD: turtle_pen(1); break;
        case OP_HOME: turtle_home(); break;
        case OP_MOVETO:
            sp -= 2;
            turtle_moveto(&stack[sp + 1], &stack[sp + 2]);
            break;
        case OP_HLT:
            return E_OK;
        default:
            return E_BAD_OP;
        }
    }
    (void)one_tmp;
}
