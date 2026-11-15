#include "core/lexer.h"
#include "core/errors.h"
#include <string.h>

#define TEXT_MAX 16

typedef struct { const char *name; uint8_t tok; } Keyword;

static const Keyword keywords[] = {
    { "abs", T_ABS }, { "atan", T_ATAN }, { "begin", T_BEGIN }, { "call", T_CALL },
    { "const", T_CONST }, { "cos", T_COS }, { "do", T_DO }, { "end", T_END },
    { "exp", T_EXP }, { "forward", T_FORWARD }, { "home", T_HOME }, { "if", T_IF },
    { "int", T_INT }, { "log", T_LOG }, { "moveto", T_MOVETO }, { "odd", T_ODD },
    { "pendown", T_PENDOWN }, { "penup", T_PENUP }, { "procedure", T_PROCEDURE },
    { "sin", T_SIN }, { "sqrt", T_SQRT }, { "then", T_THEN }, { "turn", T_TURN },
    { "var", T_VAR }, { "while", T_WHILE }
};

static int is_digit(char c) { return c >= '0' && c <= '9'; }
static int is_alpha(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }

void lex_init(Lexer *lx, const char *src) {
    lx->src = src;
    lx->pos = 0;
    lx->line = 1;
    lx->tok = T_EOF;
    lx->text[0] = 0;
    lx->err = E_NONE;
}

static void fail(Lexer *lx, uint8_t code) {
    lx->tok = T_ERROR;
    lx->err = code;
}

void lex_next(Lexer *lx) {
    const char *s = lx->src;
    char c;
    uint8_t n;

    lx->err = E_NONE;
    lx->text[0] = 0;
    for (;;) {
        c = s[lx->pos];
        if (c == '\n') { lx->line++; lx->pos++; }
        else if (c == ' ' || c == '\t' || c == '\r') lx->pos++;
        else if (c == '{') {
            uint16_t start = lx->line;
            lx->pos++;
            while (s[lx->pos] && s[lx->pos] != '}') {
                if (s[lx->pos] == '\n') lx->line++;
                lx->pos++;
            }
            if (!s[lx->pos]) { lx->line = start; fail(lx, E_COMMENT); return; }
            lx->pos++;
        } else break;
    }

    if (!c) { lx->tok = T_EOF; return; }

    if (is_alpha(c)) {
        n = 0;
        while (is_alpha(s[lx->pos]) || is_digit(s[lx->pos])) {
            char d = s[lx->pos++];
            if (n >= TEXT_MAX) { fail(lx, E_IDENT_LONG); lx->text[0] = 0; goto skip_ident; }
            lx->text[n++] = (d >= 'A' && d <= 'Z') ? (char)(d + 32) : d;
        }
        lx->text[n] = 0;
        lx->tok = T_IDENT;
        {
            uint8_t i;
            for (i = 0; i < sizeof keywords / sizeof keywords[0]; i++)
                if (strcmp(lx->text, keywords[i].name) == 0) { lx->tok = (Token)keywords[i].tok; break; }
        }
        return;
    skip_ident:
        while (is_alpha(s[lx->pos]) || is_digit(s[lx->pos])) lx->pos++;
        return;
    }

    if (is_digit(c)) {
        n = 0;
        while (is_digit(s[lx->pos])) {
            if (n >= TEXT_MAX) { fail(lx, E_CHAR); return; }
            lx->text[n++] = s[lx->pos++];
        }
        if (s[lx->pos] == '.' && is_digit(s[lx->pos + 1])) {
            if (n >= TEXT_MAX) { fail(lx, E_CHAR); return; }
            lx->text[n++] = s[lx->pos++];
            while (is_digit(s[lx->pos])) {
                if (n >= TEXT_MAX) { fail(lx, E_CHAR); return; }
                lx->text[n++] = s[lx->pos++];
            }
        }
        lx->text[n] = 0;
        if (num_parse(lx->text, &lx->value) != 0) { fail(lx, E_CHAR); return; }
        lx->tok = T_NUMBER;
        return;
    }

    if (c == '\'') {
        lx->pos++;
        n = 0;
        while (s[lx->pos] != '\'') {
            if (!s[lx->pos] || s[lx->pos] == '\n') { fail(lx, E_STRING); return; }
            if (n >= TEXT_MAX) { fail(lx, E_STRING_LONG); return; }
            lx->text[n++] = s[lx->pos++];
        }
        lx->pos++;
        lx->text[n] = 0;
        lx->tok = T_STRING;
        return;
    }

    lx->pos++;
    switch (c) {
    case '+': lx->tok = T_PLUS; break;
    case '-': lx->tok = T_MINUS; break;
    case '*': lx->tok = T_STAR; break;
    case '/': lx->tok = T_SLASH; break;
    case '(': lx->tok = T_LPAREN; break;
    case ')': lx->tok = T_RPAREN; break;
    case ',': lx->tok = T_COMMA; break;
    case ';': lx->tok = T_SEMI; break;
    case '.':
        if (is_digit(s[lx->pos])) fail(lx, E_CHAR); /* leading .5 not allowed */
        else lx->tok = T_PERIOD;
        break;
    case '=': lx->tok = T_EQ; break;
    case '#': lx->tok = T_NE; break;
    case '!': lx->tok = T_BANG; break;
    case '?': lx->tok = T_QUERY; break;
    case ':':
        if (s[lx->pos] == '=') { lx->pos++; lx->tok = T_ASSIGN; } else fail(lx, E_CHAR);
        break;
    case '<':
        if (s[lx->pos] == '=') { lx->pos++; lx->tok = T_LE; }
        else if (s[lx->pos] == '>') { lx->pos++; lx->tok = T_NE; }
        else lx->tok = T_LT;
        break;
    case '>':
        if (s[lx->pos] == '=') { lx->pos++; lx->tok = T_GE; } else lx->tok = T_GT;
        break;
    default: fail(lx, E_CHAR); break;
    }
}
