#ifndef PL0_ASCII_H
#define PL0_ASCII_H

/* ASCII is the text encoding of every portable layer: the source buffer,
 * lexer, compiler, VM strings, error texts and num_format/num_parse.
 * PETSCII and screen codes exist only at the C64's hardware boundaries
 * (keyboard in, screen out, disk files: src/c64/).
 *
 * cc65 translates C character and string literals to PETSCII for -t c64
 * unless the ASCII charmap is active; the Makefile compiles every C64 C
 * source with <ascii_charmap.h> included first. This typedef stops a build
 * that forgot it (array of size -1). Note: cc65's <ctype.h> follows the
 * target charset (isalpha(0x61) is false on the C64), so portable code
 * classifies characters with the explicit ranges below, never <ctype.h>.
 * The macros evaluate their argument more than once. */
/* XOR, not ==: cc65 2.18 rejects comparisons in an array size. */
typedef char pl0_ascii_charmap_check[1 - 2 * (('a' ^ 0x61) | ('z' ^ 0x7A) | ('A' ^ 0x41) |
                                              ('E' ^ 0x45) | ('\n' ^ 0x0A) | ('0' ^ 0x30))];

#define ASCII_IS_DIGIT(c) ((c) >= 0x30 && (c) <= 0x39)                     /* 0-9 */
#define ASCII_IS_UPPER(c) ((c) >= 0x41 && (c) <= 0x5A)                     /* A-Z */
#define ASCII_IS_LOWER(c) ((c) >= 0x61 && (c) <= 0x7A)                     /* a-z */
#define ASCII_IS_ALPHA(c) (ASCII_IS_UPPER(c) || ASCII_IS_LOWER(c))
/* Lower case: `if (ASCII_IS_UPPER(c)) c += 0x20;` -- written as an if, not
 * as `c = ... ? c + 0x20 : c`, which cc65 2.18 -O can miscompile when c is
 * a stack local (tests/c64/cc65_bug_ternary.c). */
#define ASCII_CASE_BIT 0x20

#endif
