#include "core/errors.h"
#include "core/ascii.h"     /* the texts are ASCII on every target */

/* One table indexed by error code. Keep texts <= 36 chars.
   tools/check_error_docs.py parses the string literals in this table. */
static const char *const err_msgs[E_COUNT] = {
    /* E_NONE */            "ok",
    /* E_COMMENT */         "unterminated comment",
    /* E_STRING */          "unterminated string",
    /* E_CHAR */            "bad character or number",
    /* E_IDENT_LONG */      "name too long",
    /* E_EXPECTED_SEMI */   "\";\" expected",
    /* E_EXPECTED_END */    "\"end\" expected",
    /* E_EXPECTED_PERIOD */ "\".\" expected",
    /* E_EXPECTED_THEN */   "\"then\" expected",
    /* E_EXPECTED_DO */     "\"do\" expected",
    /* E_EXPECTED_IDENT */  "identifier expected",
    /* E_EXPECTED_EQ */     "\"=\" expected",
    /* E_EXPECTED_ASSIGN */ "\":=\" expected",
    /* E_EXPECTED_LPAREN */ "\"(\" expected",
    /* E_EXPECTED_RPAREN */ "\")\" expected",
    /* E_EXPECTED_EXPR */   "expression expected",
    /* E_UNDECLARED */      "undeclared name",
    /* E_NOT_VAR */         "not a variable",
    /* E_NOT_PROC */        "not a procedure",
    /* E_REDECLARED */      "name already declared",
    /* E_ARGS */            "wrong number of arguments",
    /* E_TOO_MANY_IDENTS */ "too many names",
    /* E_PROGRAM_TOO_BIG */ "program too big",
    /* E_NESTING */         "nested too deeply",
    /* E_DIV_ZERO */        "division by zero",
    /* E_DOMAIN */          "domain error",
    /* E_OVERFLOW */        "number overflow",
    /* E_STACK */           "stack overflow (too deep)",
    /* E_STOPPED */         "stopped",
    /* E_BAD_INPUT */       "bad input",
    /* E_STRING_LONG */     "string too long (max 16)"
};

const char *err_text(uint8_t code) {
    if (code < E_COUNT) return err_msgs[code];
    return "unknown error";
}
