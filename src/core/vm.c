/* P-code interpreter.
 *
 * Written in the cc65-friendly style the Task 1 spike measured (section 8 of
 * docs/architecture/spike-results.md): num_t pointers for the stack, byte
 * offsets pre-scaled by the compiler (no multiply by 5 per access), a display
 * disp[] indexed by absolute declaration level instead of a static-link walk,
 * num_copy instead of struct assignment, state in statics.
 *
 * Stack: sp points to the next free cell. The stack is checked only by INT,
 * which reserves the frame and verifies room for the block's maximum number
 * of temporaries (computed by the compiler), so pushes need no check. */
#include <string.h>
#include "core/vm.h"
#include "platform/io.h"

#define CELL_AT(base, off) ((num_t *)((char *)(base) + (off)))

static num_t stack[MAX_STACK];
static num_t *disp[MAX_NEST + 1];   /* frame base per declaration level */
static char numbuf[16];

static int fail(PL0Error *err, uint8_t code, int16_t line)
{
    err->code = code;
    err->line = (uint16_t)line;
    return 1;
}

int pl0_run(const Program *p, PL0Error *err)
{
    const Instr *ip = p->code;
    num_t *sp = stack;

    err->code = E_NONE;
    err->line = 0;
    disp[0] = stack;
    for (;;) {
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
        case OP_INT:
            if ((size_t)((char *)(stack + MAX_STACK) - (char *)sp) <
                (size_t)ip->arg + (size_t)ip->lvl * sizeof(num_t))
                return fail(err, E_STACK, 0);
            memset(sp, 0, (size_t)ip->arg);   /* variables start at 0 */
            sp = CELL_AT(sp, ip->arg);
            break;
        case OP_OPR:
            switch (ip->lvl) {
            case OPR_RET:
                /* Only the main program returns until Task 7, which pops the
                 * frame (DL, RA, saved display entry) unless bp == stack. */
                return 0;
            case OPR_NEG:
                num_neg(sp - 1, sp - 1);
                break;
            case OPR_ADD:
                --sp;
                num_add(sp - 1, sp - 1, sp);
                break;
            case OPR_SUB:
                --sp;
                num_sub(sp - 1, sp - 1, sp);
                break;
            case OPR_MUL:
                --sp;
                num_mul(sp - 1, sp - 1, sp);
                break;
            case OPR_DIV:
                --sp;
                if (num_div(sp - 1, sp - 1, sp)) return fail(err, E_DIV_ZERO, ip->arg);
                break;
            }
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
