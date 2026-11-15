#ifndef PL0_LIMITS_H
#define PL0_LIMITS_H

#define MAX_CODE    768   /* max compiled code size (instructions/bytes) */
#define MAX_IDENTS  64    /* max declared names in the symbol table */
#define MAX_STACK   512   /* max runtime stack size (cells) */
#define MAX_NEST    4     /* max procedure nesting depth */
#define MAX_SOURCE  8192  /* max source text size in bytes */
#define MAX_CONSTS  128   /* max numeric constants in the constant pool */
#define MAX_STRINGS 1024  /* max bytes of string literal storage */
#define MAX_EXPR_NEST 16  /* max nesting of (...) and unary signs; bounds the
                             compiler's recursion on the 6502 hardware stack */

#endif
