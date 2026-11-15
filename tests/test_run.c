#include "test.h"
#include "core/compiler.h"
#include "core/vm.h"
#include "core/pcode.h"
#include "core/errors.h"
#include "core/limits.h"
#include "host/io_host.h"
#include <stdio.h>
#include <string.h>

static Program prog;
static char result[256];
static char big[MAX_SOURCE * 2];

/* Compile and run; returns captured output or "ERROR line N: msg". */
static const char *run(const char *src) {
    PL0Error e;
    io_capture_begin();
    if (pl0_compile(src, &prog, &e) != 0 || pl0_run(&prog, &e) != 0) {
        sprintf(result, "ERROR line %u: %s", (unsigned)e.line, err_text(e.code));
        return result;
    }
    return io_capture_get();
}

static void output_literal(void) { CHECK_EQ_STR(run("! 1+2*3."), "7"); }
static void precedence_and_parens(void) { CHECK_EQ_STR(run("!(1+2)*3."), "9"); }
static void unary_minus(void) {
    CHECK_EQ_STR(run("!-2+5."), "3");
    CHECK_EQ_STR(run("!2*-3."), "-6");
    CHECK_EQ_STR(run("!--4."), "4");
    CHECK_EQ_STR(run("!+4-1."), "3");
}
static void const_and_var(void) {
    CHECK_EQ_STR(run("const a=4; var b; begin b:=a*a; !b end."), "16");
    CHECK_EQ_STR(run("const a=1, b=-2.5; var x, y; begin x:=a; y:=b; !x; !' '; !y end."), "1 -2.5");
}
static void string_output(void) {
    CHECK_EQ_STR(run("!'hi'."), "hi");
    CHECK_EQ_STR(run("begin !'a = '; !1 end."), "a = 1");
}
static void bang_alone_newline(void) {
    CHECK_EQ_STR(run("begin !1; !; !2 end."), "1\n2");
    CHECK_EQ_STR(run("!."), "\n");
    CHECK_EQ_STR(run("begin ! end."), "\n");
}
static void outputs_not_separated(void) { CHECK_EQ_STR(run("begin ! 1; ! 2 end."), "12"); }
static void float_sum(void) { CHECK_EQ_STR(run("!0.1+0.2."), "0.3"); }
static void division(void) { CHECK_EQ_STR(run("!7/2."), "3.5"); CHECK_EQ_STR(run("!8/2/2."), "2"); }
static void subtraction_left_assoc(void) { CHECK_EQ_STR(run("!10-3-2."), "5"); }
static void empty_program(void) { CHECK_EQ_STR(run("."), ""); }
static void comment_only_then_period(void) { CHECK_EQ_STR(run("{x} ."), ""); }
static void missing_period(void) { CHECK_EQ_STR(run("begin end"), "ERROR line 1: \".\" expected"); }
static void undeclared(void) { CHECK_EQ_STR(run("!x."), "ERROR line 1: undeclared name"); }
static void assign_to_const(void) {
    PL0Error e;
    CHECK_EQ_INT(pl0_compile("const a=1; a:=2.", &prog, &e), 1);
    CHECK_EQ_INT(e.code, E_NOT_VAR);
    CHECK_EQ_STR(run("const a=1; a:=2."), "ERROR line 1: not a variable");
}
static void redeclared(void) {
    CHECK_EQ_STR(run("var a, a; ."), "ERROR line 1: name already declared");
    CHECK_EQ_STR(run("const a=1; var a; ."), "ERROR line 1: name already declared");
}
static void error_line_number(void) {
    CHECK_EQ_STR(run("var a;\nbegin\n  a := b\nend."), "ERROR line 3: undeclared name");
    CHECK_EQ_STR(run("var a;\nbegin\n  a := 1\n  !a\nend."), "ERROR line 4: \";\" expected");
}
static void too_many_idents(void) {
    char *p;
    int i;
    strcpy(big, "var v0");
    p = big + strlen(big);
    for (i = 1; i < MAX_IDENTS; i++) p += sprintf(p, ", v%d", i);
    strcpy(p, "; .");
    CHECK_EQ_STR(run(big), "");                        /* exactly MAX_IDENTS: ok */
    sprintf(p, ", v%d; .", MAX_IDENTS);
    CHECK_EQ_STR(run(big), "ERROR line 1: too many names");
}
static void program_too_big(void) {
    char *p = big;
    int i;
    p += sprintf(p, "begin");
    for (i = 0; i < MAX_CODE; i++) p += sprintf(p, " !1;");
    strcpy(p, " end.");
    CHECK_EQ_STR(run(big), "ERROR line 1: program too big");
}
static void expression_nesting_limit(void) {
    char *p = big;
    int i;
    *p++ = '!';
    for (i = 0; i < 40; i++) *p++ = '(';
    *p++ = '1';
    for (i = 0; i < 40; i++) *p++ = ')';
    strcpy(p, ".");
    CHECK_EQ_STR(run(big), "ERROR line 1: program too big");
    CHECK_EQ_STR(run("!((((((((1)))))))).") , "1");
}
static void literals_deduplicated(void) {
    PL0Error e;
    CHECK_EQ_INT(pl0_compile("const a=2; !1+1+2+a+1.", &prog, &e), 0);
    CHECK_EQ_INT(prog.nconsts, 2);
}
static void lexer_error_reported(void) {
    CHECK_EQ_STR(run("\n!'abc."), "ERROR line 2: unterminated string");
    CHECK_EQ_STR(run("{ never closed"), "ERROR line 1: unterminated comment");
}
static void syntax_errors(void) {
    CHECK_EQ_STR(run("var ; ."), "ERROR line 1: identifier expected");
    CHECK_EQ_STR(run("const a 1; ."), "ERROR line 1: \"=\" expected");
    CHECK_EQ_STR(run("var a; a = 1."), "ERROR line 1: \":=\" expected");
    CHECK_EQ_STR(run("!(1+2."), "ERROR line 1: \")\" expected");
    CHECK_EQ_STR(run("!1+."), "ERROR line 1: expression expected");
    CHECK_EQ_STR(run("begin !1 end end."), "ERROR line 1: \".\" expected");
    CHECK_EQ_STR(run("begin !1 ."), "ERROR line 1: \"end\" expected");
    CHECK_EQ_STR(run("var a b; ."), "ERROR line 1: \";\" expected");
}
static void div_zero_runtime_line(void) {
    CHECK_EQ_STR(run("var a;\nbegin\n  a := 0;\n  !1/a\nend."), "ERROR line 4: division by zero");
}
static void vars_start_at_zero(void) { CHECK_EQ_STR(run("var a; !a."), "0"); }

int main(void) {
    output_literal();
    precedence_and_parens();
    unary_minus();
    const_and_var();
    string_output();
    bang_alone_newline();
    outputs_not_separated();
    float_sum();
    division();
    subtraction_left_assoc();
    empty_program();
    comment_only_then_period();
    missing_period();
    undeclared();
    assign_to_const();
    redeclared();
    error_line_number();
    too_many_idents();
    program_too_big();
    expression_nesting_limit();
    literals_deduplicated();
    lexer_error_reported();
    syntax_errors();
    div_zero_runtime_line();
    vars_start_at_zero();
    TEST_MAIN_END();
}
