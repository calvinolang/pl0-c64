#ifndef PL0_VM_H
#define PL0_VM_H
#include "core/pcode.h"
#include "core/errors.h"

/* Run a compiled program. Returns 0 when it finishes, nonzero on a
 * runtime error (err->code, err->line set). */
int pl0_run(const Program *p, PL0Error *err);

#ifdef PL0_TEST_HOOKS
/* Test builds only: the highest stack top (in cells) the last pl0_run
 * reached. It never exceeds MAX_STACK. */
unsigned vm_stack_high_water(void);
/* Test builds only: P-code instructions the last pl0_run executed
 * (every dispatched instruction, the final RET of the main program too). */
unsigned long vm_instr_count(void);
/* Test builds only: how many of them were FN (math function) calls. */
unsigned long vm_fn_count(void);
#endif

#endif
