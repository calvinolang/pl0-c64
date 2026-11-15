#ifndef PL0_ERRORS_H
#define PL0_ERRORS_H
#include <stdint.h>

enum {
    E_NONE = 0, E_COMMENT, E_STRING, E_CHAR, E_IDENT_LONG,
    E_EXPECTED_SEMI, E_EXPECTED_END, E_EXPECTED_PERIOD, E_EXPECTED_THEN,
    E_EXPECTED_DO, E_EXPECTED_IDENT, E_EXPECTED_EQ, E_EXPECTED_ASSIGN,
    E_EXPECTED_LPAREN, E_EXPECTED_RPAREN, E_EXPECTED_EXPR,
    E_UNDECLARED, E_NOT_VAR, E_NOT_PROC, E_REDECLARED, E_ARGS,
    E_TOO_MANY_IDENTS, E_PROGRAM_TOO_BIG, E_NESTING,
    E_DIV_ZERO, E_DOMAIN, E_OVERFLOW, E_STACK, E_STOPPED, E_BAD_INPUT,
    E_COUNT /* number of codes; keep last */
};

typedef struct { uint8_t code; uint16_t line; } PL0Error;

/* Plain-language message for a code; "unknown error" if out of range. */
const char *err_text(uint8_t code);

#endif
