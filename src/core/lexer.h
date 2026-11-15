#ifndef PL0_LEXER_H
#define PL0_LEXER_H
#include <stdint.h>
#include "platform/num.h"

typedef enum { T_EOF, T_IDENT, T_NUMBER, T_STRING, /* keywords */ T_CONST, T_VAR, T_PROCEDURE, T_CALL,
  T_BEGIN, T_END, T_IF, T_THEN, T_WHILE, T_DO, T_ODD, T_FORWARD, T_TURN, T_PENUP, T_PENDOWN, T_HOME, T_MOVETO,
  T_SIN, T_COS, T_ATAN, T_SQRT, T_ABS, T_INT, T_EXP, T_LOG,
  /* symbols */ T_PLUS, T_MINUS, T_STAR, T_SLASH, T_LPAREN, T_RPAREN, T_COMMA, T_SEMI, T_PERIOD,
  T_ASSIGN, T_EQ, T_NE, T_LT, T_LE, T_GT, T_GE, T_BANG, T_QUERY, T_ERROR } Token;

typedef struct { const char *src; uint16_t pos; uint16_t line; Token tok; char text[17]; num_t value; uint8_t err; } Lexer;

void lex_init(Lexer *lx, const char *src);
void lex_next(Lexer *lx);   /* advances to next token; sets lx->err (error code) on bad input */

#endif
