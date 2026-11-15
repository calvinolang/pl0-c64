#ifndef PL0_COMPILER_H
#define PL0_COMPILER_H
#include "core/pcode.h"
#include "core/errors.h"

/* Compile PL/0 source into p. Stops at the first error.
 * Returns 0 on success, nonzero on error (err->code, err->line set). */
int pl0_compile(const char *src, Program *p, PL0Error *err);

#endif
