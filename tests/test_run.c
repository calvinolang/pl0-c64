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
/* "!" followed by n nested parentheses around 1. */
static const char *parens(int n) {
    char *p = big;
    int i;
    *p++ = '!';
    for (i = 0; i < n; i++) *p++ = '(';
    *p++ = '1';
    for (i = 0; i < n; i++) *p++ = ')';
    strcpy(p, ".");
    return big;
}
static void expression_nesting_limit(void) {
    CHECK_EQ_STR(run(parens(MAX_EXPR_NEST)), "1");               /* 16: ok */
    CHECK_EQ_STR(run(parens(MAX_EXPR_NEST + 1)), "ERROR line 1: nested too deeply");
    CHECK_EQ_STR(run(parens(40)), "ERROR line 1: nested too deeply");
    /* unary signs count too: 16 minus signs ok, 17 too deep */
    memset(big, '-', MAX_EXPR_NEST + 1);
    big[0] = '!';
    strcpy(big + 1 + MAX_EXPR_NEST, "1.");
    CHECK_EQ_STR(run(big), "1");                                 /* 16 signs: even */
    memset(big, '-', MAX_EXPR_NEST + 2);
    big[0] = '!';
    strcpy(big + 2 + MAX_EXPR_NEST, "1.");
    CHECK_EQ_STR(run(big), "ERROR line 1: nested too deeply");
}
/* n nested "begin ... end" around "!1". */
static const char *begins(int n) {
    char *p = big;
    int i;
    for (i = 0; i < n; i++) p += sprintf(p, "begin ");
    p += sprintf(p, "!1");
    for (i = 0; i < n; i++) p += sprintf(p, " end");
    strcpy(p, ".");
    return big;
}
static void statement_nesting_limit(void) {
    PL0Error e;
    CHECK_EQ_STR(run(begins(MAX_STMT_NEST)), "1");               /* 16: ok */
    CHECK_EQ_STR(run(begins(MAX_STMT_NEST + 1)), "ERROR line 1: nested too deeply");
    CHECK_EQ_STR(run(begins(200)), "ERROR line 1: nested too deeply");
    CHECK_EQ_INT(pl0_compile(begins(200), &prog, &e), 1);
    CHECK_EQ_INT(e.code, E_NESTING);
}
static void consts_limit(void) {
    char *p;
    int i;
    PL0Error e;
    /* MAX_CONSTS distinct literals ok, one more -> program too big */
    p = big + sprintf(big, "begin !1");
    for (i = 2; i <= MAX_CONSTS; i++) p += sprintf(p, "; !%d", i);
    strcpy(p, " end.");
    CHECK_EQ_INT(pl0_compile(big, &prog, &e), 0);
    CHECK_EQ_INT(prog.nconsts, MAX_CONSTS);
    sprintf(p, "; !%d end.", MAX_CONSTS + 1);
    CHECK_EQ_STR(run(big), "ERROR line 1: program too big");
}
static void strings_limit(void) {
    char *p;
    int i;
    PL0Error e;
    /* 60 strings of 16 chars use 60*17 = 1020 bytes; a 3-char string
       (4 bytes with its NUL) fills the pool to exactly MAX_STRINGS = 1024. */
    p = big + sprintf(big, "begin !'s%015d'", 0);
    for (i = 1; i < 60; i++) p += sprintf(p, "; !'s%015d'", i);
    strcpy(p, "; !'abc' end.");
    CHECK_EQ_INT(pl0_compile(big, &prog, &e), 0);
    CHECK_EQ_INT(prog.nstrings, MAX_STRINGS);
    strcpy(p, "; !'abc'; !'x' end.");                  /* 2 more bytes */
    CHECK_EQ_STR(run(big), "ERROR line 1: program too big");
}
static void code_limit(void) {
    char *p;
    int i;
    PL0Error e;
    /* INT + n bare "!" (one WRL each) + RET = n + 2 instructions */
    p = big + sprintf(big, "begin !");
    for (i = 1; i < MAX_CODE - 2; i++) p += sprintf(p, "; !");
    strcpy(p, " end.");
    CHECK_EQ_INT(pl0_compile(big, &prog, &e), 0);
    CHECK_EQ_INT(prog.ncode, MAX_CODE);
    strcpy(p, "; ! end.");
    CHECK_EQ_INT(pl0_compile(big, &prog, &e), 1);
    CHECK_EQ_INT(e.code, E_PROGRAM_TOO_BIG);
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

static void if_true(void) { CHECK_EQ_STR(run("if 1=1 then !5."), "5"); }
static void if_false(void) {
    CHECK_EQ_STR(run("if 1=2 then !5."), "");
    CHECK_EQ_STR(run("begin if 1=2 then !5; !6 end."), "6");
}
static void while_sum(void) {
    CHECK_EQ_STR(run("var i, s; begin i:=1; s:=0; while i<=10 do begin s:=s+i; i:=i+1 end; !s end."), "55");
}
static void odd_cond(void) {
    CHECK_EQ_STR(run("if odd 3 then !1."), "1");
    CHECK_EQ_STR(run("if odd 4 then !1."), "");
    CHECK_EQ_STR(run("if odd 2.5 then !1."), "");
    CHECK_EQ_STR(run("if odd 1+2 then !1."), "1");
}
static void all_relops(void) {
    CHECK_EQ_STR(run("begin if 1=2 then !1; if 1#2 then !2; if 1<>2 then !3; if 1<2 then !4;"
                     " if 1<=2 then !5; if 1>2 then !6; if 1>=2 then !7 end."), "2345");
    CHECK_EQ_STR(run("begin if 2=2 then !1; if 2#2 then !2; if 2<>2 then !3; if 2<2 then !4;"
                     " if 2<=2 then !5; if 2>2 then !6; if 2>=2 then !7 end."), "157");
    CHECK_EQ_STR(run("begin if 2=1 then !1; if 2>1 then !2; if 2>=1 then !3; if 2<1 then !4;"
                     " if 2<=1 then !5 end."), "23");
}
static void nested_while(void) {
    CHECK_EQ_STR(run("var i, j, n; begin n:=0; i:=0; while i<3 do begin j:=0;"
                     " while j<4 do begin n:=n+1; j:=j+1 end; i:=i+1 end; !n end."), "12");
}
static void while_missing_do(void) {
    CHECK_EQ_STR(run("while 1=2 !1."), "ERROR line 1: \"do\" expected");
}
static void if_missing_then(void) {
    CHECK_EQ_STR(run("if 1=1 !1."), "ERROR line 1: \"then\" expected");
}
static void missing_relop(void) {
    CHECK_EQ_STR(run("if 1 then !1."), "ERROR line 1: expression expected");
}
static void float_compare(void) {
    CHECK_EQ_STR(run("if 0.5 < 0.75 then !1."), "1");
    CHECK_EQ_STR(run("if 0.75 < 0.5 then !1."), "");
}
static void sequential_ifs_while(void) {
    int i;
    char *p = big;
    p += sprintf(p, "begin ");
    for (i = 0; i < 40; i++) p += sprintf(p, "if 1=1 then !1; while 1=2 do !2; ");
    strcpy(p, "! 3 end.");
    CHECK_EQ_STR(run(big), "1111111111111111111111111111111111111111" "3");
}
static const char *nested(const char *kw, int n) {
    char *p = big;
    int i;
    for (i = 0; i < n; i++) p += sprintf(p, "%s 1=0 %s ", kw, kw[0] == 'w' ? "do" : "then");
    strcpy(p, "!1.");
    return big;
}
static void if_while_nesting_limit(void) {
    CHECK_EQ_STR(run(nested("while", MAX_STMT_NEST)), "");
    CHECK_EQ_STR(run(nested("while", MAX_STMT_NEST + 1)), "ERROR line 1: nested too deeply");
    CHECK_EQ_STR(run(nested("if", MAX_STMT_NEST)), "");
    CHECK_EQ_STR(run(nested("if", MAX_STMT_NEST + 1)), "ERROR line 1: nested too deeply");
}
static void while_stopped(void) {
    io_host_break_after(5);
    CHECK_EQ_STR(run("var x;\nbegin\n  x := 0;\n  while 1=1 do x := x + 1\nend."), "ERROR line 4: stopped");
    io_host_break_after(3);
    CHECK_EQ_STR(run("while 1=1 do."), "ERROR line 1: stopped");
    io_host_break_after(0);
}
static void runtime_error_in_loop(void) {
    CHECK_EQ_STR(run("var i;\nbegin\n  i := 2;\n  while i >= 0 do\n  begin\n    !10/i;\n    i := i - 1\n  end\nend."),
                 "ERROR line 6: division by zero");
}

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
    statement_nesting_limit();
    consts_limit();
    strings_limit();
    code_limit();
    literals_deduplicated();
    lexer_error_reported();
    syntax_errors();
    div_zero_runtime_line();
    vars_start_at_zero();
    if_true();
    if_false();
    while_sum();
    odd_cond();
    all_relops();
    nested_while();
    while_missing_do();
    if_missing_then();
    missing_relop();
    float_compare();
    sequential_ifs_while();
    if_while_nesting_limit();
    while_stopped();
    runtime_error_in_loop();
    TEST_MAIN_END();
}
