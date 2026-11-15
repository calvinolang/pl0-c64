#ifndef PL0_LIMITS_H
#define PL0_LIMITS_H

#define MAX_CODE    768   /* max compiled code size (instructions/bytes) */
#define MAX_IDENTS  64    /* max declared names in the symbol table */
#define MAX_STACK   512   /* max runtime stack size (cells) */
#define MAX_NEST    4     /* max procedure nesting depth */
#define MAX_SOURCE  8192  /* max source text size in bytes */
#define MAX_CONSTS  128   /* max numeric constants in the constant pool */
#define MAX_STRINGS 1024  /* max bytes of string literal storage */
#define MAX_EXPR_NEST 16  /* max nesting of (...) and unary signs */
#define MAX_STMT_NEST 16  /* max nesting of begin (later if/while bodies) */
/* The two NEST limits bound the compiler's recursion on the cc65 stacks;
   exceeding either (or MAX_NEST) reports E_NESTING. */

#endif
