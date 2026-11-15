#ifndef PL0_VM_H
#define PL0_VM_H
#include "core/pcode.h"
#include "core/errors.h"

/* Run a compiled program. Returns 0 when it finishes, nonzero on a
 * runtime error (err->code, err->line set). */
int pl0_run(const Program *p, PL0Error *err);

#endif
