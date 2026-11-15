#include "test.h"
#include "core/lexer.h"
#include "core/errors.h"
#include "core/ascii.h"
#include <ctype.h>
#include <string.h>

static void first(Lexer *lx, const char *s) { lex_init(lx, s); lex_next(lx); }

static void keywords_case_insensitive(void) {
    Lexer lx;
    first(&lx, "BEGIN"); CHECK_EQ_INT(lx.tok, T_BEGIN);
    first(&lx, "Begin"); CHECK_EQ_INT(lx.tok, T_BEGIN);
    first(&lx, "begin"); CHECK_EQ_INT(lx.tok, T_BEGIN);
    first(&lx, "MoveTo"); CHECK_EQ_INT(lx.tok, T_MOVETO);
    first(&lx, "Foo"); CHECK_EQ_INT(lx.tok, T_IDENT); CHECK(strcmp(lx.text, "foo") == 0);
    first(&lx, "beginx"); CHECK_EQ_INT(lx.tok, T_IDENT);
}

static void numbers(void) {
    Lexer lx;
    first(&lx, "12"); CHECK_EQ_INT(lx.tok, T_NUMBER); CHECK_NEAR(lx.value, 12, 1e-9);
    first(&lx, "3.14"); CHECK_EQ_INT(lx.tok, T_NUMBER); CHECK_NEAR(lx.value, 3.14, 1e-9);
    first(&lx, ".5"); CHECK_EQ_INT(lx.tok, T_ERROR); CHECK(lx.err != E_NONE);
    /* '.' belongs to number only if a digit follows */
    lex_init(&lx, "3.");
    lex_next(&lx); CHECK_EQ_INT(lx.tok, T_NUMBER); CHECK_NEAR(lx.value, 3, 1e-9);
    lex_next(&lx); CHECK_EQ_INT(lx.tok, T_PERIOD);
    lex_next(&lx); CHECK_EQ_INT(lx.tok, T_EOF);
}

static void comment_skipped(void) {
    Lexer lx;
    first(&lx, "{ a } x"); CHECK_EQ_INT(lx.tok, T_IDENT); CHECK(strcmp(lx.text, "x") == 0);
}

static void unterminated_comment(void) {
    Lexer lx;
    first(&lx, "{ abc"); CHECK_EQ_INT(lx.tok, T_ERROR); CHECK_EQ_INT(lx.err, E_COMMENT);
}

static void strings(void) {
    Lexer lx;
    first(&lx, "'Hi There'"); CHECK_EQ_INT(lx.tok, T_STRING); CHECK(strcmp(lx.text, "Hi There") == 0);
    first(&lx, "''"); CHECK_EQ_INT(lx.tok, T_STRING); CHECK(lx.text[0] == 0);
    first(&lx, "'1234567890123456'"); CHECK_EQ_INT(lx.tok, T_STRING);
    first(&lx, "'12345678901234567'"); CHECK_EQ_INT(lx.tok, T_ERROR); CHECK_EQ_INT(lx.err, E_STRING_LONG);
}

static void unterminated_string(void) {
    Lexer lx;
    first(&lx, "'abc"); CHECK_EQ_INT(lx.tok, T_ERROR); CHECK_EQ_INT(lx.err, E_STRING);
    first(&lx, "'abc\nx'"); CHECK_EQ_INT(lx.tok, T_ERROR); CHECK_EQ_INT(lx.err, E_STRING);
}

static void line_numbers(void) {
    Lexer lx;
    lex_init(&lx, "a\n{c\nd}\n  b");
    lex_next(&lx); CHECK_EQ_INT(lx.line, 1);
    lex_next(&lx); CHECK_EQ_INT(lx.tok, T_IDENT); CHECK_EQ_INT(lx.line, 4);
    lex_init(&lx, "a\nb\n c");
    lex_next(&lx); lex_next(&lx); lex_next(&lx); CHECK_EQ_INT(lx.line, 3);
}

static void ne_both_forms(void) {
    Lexer lx;
    first(&lx, "#"); CHECK_EQ_INT(lx.tok, T_NE);
    first(&lx, "<>"); CHECK_EQ_INT(lx.tok, T_NE);
    first(&lx, "<="); CHECK_EQ_INT(lx.tok, T_LE);
    first(&lx, "<"); CHECK_EQ_INT(lx.tok, T_LT);
    first(&lx, ">="); CHECK_EQ_INT(lx.tok, T_GE);
    first(&lx, ">"); CHECK_EQ_INT(lx.tok, T_GT);
}

static void assign_vs_eq(void) {
    Lexer lx;
    first(&lx, ":="); CHECK_EQ_INT(lx.tok, T_ASSIGN);
    first(&lx, "="); CHECK_EQ_INT(lx.tok, T_EQ);
    first(&lx, ": "); CHECK_EQ_INT(lx.tok, T_ERROR); CHECK_EQ_INT(lx.err, E_CHAR);
}

static void symbols(void) {
    Lexer lx;
    lex_init(&lx, "+-*/(),;.!?");
    Token exp[] = { T_PLUS, T_MINUS, T_STAR, T_SLASH, T_LPAREN, T_RPAREN, T_COMMA, T_SEMI, T_PERIOD, T_BANG, T_QUERY, T_EOF };
    unsigned i;
    for (i = 0; i < sizeof exp / sizeof exp[0]; i++) { lex_next(&lx); CHECK_EQ_INT(lx.tok, exp[i]); }
}

static void identifier_too_long(void) {
    Lexer lx;
    first(&lx, "abcdefghijklmnop"); CHECK_EQ_INT(lx.tok, T_IDENT); /* 16 ok */
    first(&lx, "abcdefghijklmnopq"); CHECK_EQ_INT(lx.tok, T_ERROR); CHECK_EQ_INT(lx.err, E_IDENT_LONG);
}

static void empty_source(void) {
    Lexer lx;
    first(&lx, ""); CHECK_EQ_INT(lx.tok, T_EOF);
    lex_next(&lx); CHECK_EQ_INT(lx.tok, T_EOF);
    first(&lx, "  \n "); CHECK_EQ_INT(lx.tok, T_EOF);
}

static void unknown_char(void) {
    Lexer lx;
    first(&lx, "@"); CHECK_EQ_INT(lx.tok, T_ERROR); CHECK_EQ_INT(lx.err, E_CHAR);
}

/* core/ascii.h against the host's <ctype.h> in the C locale (ASCII), for
 * every byte value: nothing at or above 0x80 (PETSCII letters, Latin-1) is
 * a letter or digit. */
static void ascii_classes(void) {
    int c, bad = 0;
    for (c = 0; c < 256; c++) {
        int ascii = c < 0x80;
        if (ASCII_IS_DIGIT(c) != (ascii && isdigit(c) != 0)) bad++;
        if (ASCII_IS_UPPER(c) != (ascii && isupper(c) != 0)) bad++;
        if (ASCII_IS_LOWER(c) != (ascii && islower(c) != 0)) bad++;
        if (ASCII_IS_ALPHA(c) != (ascii && isalpha(c) != 0)) bad++;
        if (ASCII_IS_UPPER(c) && tolower(c) != c + ASCII_CASE_BIT) bad++;
    }
    CHECK_EQ_INT(bad, 0);
}

/* Identifiers fold A-Z to a-z and nothing else; the characters next to the
 * letter ranges and 8-bit codes (PETSCII shifted letters $C1-$DA) are not
 * letters. */
static void ascii_identifiers(void) {
    Lexer lx;
    first(&lx, "AZaz09"); CHECK_EQ_INT(lx.tok, T_IDENT); CHECK(strcmp(lx.text, "azaz09") == 0);
    first(&lx, "WHILE"); CHECK_EQ_INT(lx.tok, T_WHILE);
    first(&lx, "PenDown"); CHECK_EQ_INT(lx.tok, T_PENDOWN);
    first(&lx, "[");  CHECK_EQ_INT(lx.tok, T_ERROR); CHECK_EQ_INT(lx.err, E_CHAR);
    first(&lx, "`");  CHECK_EQ_INT(lx.tok, T_ERROR); CHECK_EQ_INT(lx.err, E_CHAR);
    first(&lx, "\xC1"); CHECK_EQ_INT(lx.tok, T_ERROR); CHECK_EQ_INT(lx.err, E_CHAR);
    lex_init(&lx, "a\xC1");
    lex_next(&lx); CHECK_EQ_INT(lx.tok, T_IDENT); CHECK(strcmp(lx.text, "a") == 0);
    lex_next(&lx); CHECK_EQ_INT(lx.tok, T_ERROR);
}

int main(void) {
    ascii_classes(); ascii_identifiers();
    keywords_case_insensitive(); numbers(); comment_skipped(); unterminated_comment();
    strings(); unterminated_string(); line_numbers(); ne_both_forms(); assign_vs_eq();
    symbols(); identifier_too_long(); empty_source(); unknown_char();
    TEST_MAIN_END();
}
