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
static void overflow_loop(void) {
    CHECK_EQ_STR(run("var x; begin x := 10; while 1 = 1 do x := x * x end."), "ERROR line 1: number overflow");
    /* 1e9 squared five times is 1e288: fine; one more is not (line 5) */
    CHECK_EQ_STR(run("var x;\nbegin\n x := 1000000000;\n x:=x*x; x:=x*x; x:=x*x; x:=x*x; x:=x*x;\n !x\nend."), "1E+288");
    CHECK_EQ_STR(run("var x;\nbegin\n x := 1000000000;\n x:=x*x; x:=x*x; x:=x*x; x:=x*x; x:=x*x;\n !x+x*x\nend."), "ERROR line 5: number overflow");
    CHECK_EQ_STR(run("var x;\nbegin\n x := 1000000000;\n x:=x*x; x:=x*x; x:=x*x; x:=x*x; x:=x*x;\n !x*x\nend."), "ERROR line 5: number overflow");
    CHECK_EQ_STR(run("var x;\nbegin\n x := 1000000000;\n x:=x*x; x:=x*x; x:=x*x; x:=x*x; x:=x*x;\n !x+x*1000000000*1000000000*1000000000*1000000000\nend."), "ERROR line 5: number overflow");
    CHECK_EQ_STR(run("var x;\nbegin\n x := 1000000000;\n x:=x*x; x:=x*x; x:=x*x; x:=x*x; x:=x*x;\n !x/0.0000000001/0.0000000001/0.0000000001/0.0000000001\nend."), "ERROR line 5: number overflow");
    CHECK_EQ_STR(run("var x;\nbegin\n x := 1000000000;\n x:=x*x; x:=x*x; x:=x*x; x:=x*x; x:=x*x;\n !0-x-x*x\nend."), "ERROR line 5: number overflow");
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

/* ---- Task 7: procedures ---- */
static void proc_no_params(void) {
    CHECK_EQ_STR(run("procedure p; !1; call p."), "1");
    CHECK_EQ_STR(run("procedure p; !1; begin call p; call p end."), "11");
}
static void proc_params(void) {
    CHECK_EQ_STR(run("procedure add(a,b); !a+b; call add(2,3)."), "5");
    /* arguments are evaluated left to right and bound in order */
    CHECK_EQ_STR(run("procedure sub(a,b); !a-b; call sub(10,3)."), "7");
    CHECK_EQ_STR(run("var x; procedure p(a,b,c); begin !a; !b; !c end; begin x:=2; call p(x, x*3, (x+1)*x) end."),
                 "266");
}
static void factorial_recursive(void) {
    CHECK_EQ_STR(run("var r;\n"
                     "procedure fact(n);\n"
                     "begin\n"
                     "  if n <= 1 then r := 1;\n"
                     "  if n > 1 then begin call fact(n-1); r := r*n end\n"
                     "end;\n"
                     "begin call fact(10); !r end."), "3628800");
}
static void fib_recursive(void) {
    CHECK_EQ_STR(run("var r;\n"
                     "procedure fib(n);\n"
                     "var a;\n"
                     "begin\n"
                     "  if n < 2 then r := n;\n"
                     "  if n >= 2 then begin call fib(n-1); a := r; call fib(n-2); r := r+a end\n"
                     "end;\n"
                     "begin call fib(15); !r end."), "610");
}
static void nested_proc_sees_outer_var(void) {
    CHECK_EQ_STR(run("var g;\n"
                     "procedure outer(x);\n"
                     "var y;\n"
                     "  procedure inner(z);\n"
                     "  begin !g; !' '; !x; !' '; !y; !' '; !z; y := y+100; g := g+1 end;\n"
                     "begin y := x*2; call inner(x+1); !' '; !y end;\n"
                     "begin g := 7; call outer(3); !' '; !g end."), "7 3 6 4 106 8");
    /* a nested procedure may call its enclosing procedure (display restored) */
    CHECK_EQ_STR(run("procedure outer(n);\n"
                     "  procedure inner;\n"
                     "  begin if n > 0 then call outer(n-1) end;\n"
                     "begin !n; call inner; !n end;\n"
                     "call outer(3)."), "32100123");
    /* four nested levels see every enclosing level */
    CHECK_EQ_STR(run("var a; procedure p1(b); procedure p2(c); procedure p3(d); procedure p4(e);"
                     " !a+b+c+d+e; call p4(5); call p3(4); call p2(3); begin a:=1; call p1(2) end."), "15");
}
static void params_are_by_value(void) {
    CHECK_EQ_STR(run("var a; procedure p(x); begin x := x+1; !x end; begin a := 5; call p(a); !' '; !a end."),
                 "6 5");
}
static void args_count_mismatch(void) {
    PL0Error e;
    CHECK_EQ_INT(pl0_compile("procedure p(a,b); ; call p(1).", &prog, &e), 1);
    CHECK_EQ_INT(e.code, E_ARGS);
    CHECK_EQ_STR(run("procedure p(a,b); ; call p(1)."), "ERROR line 1: wrong number of arguments");
    CHECK_EQ_STR(run("procedure p(a,b); ; call p(1,2,3)."), "ERROR line 1: wrong number of arguments");
    CHECK_EQ_STR(run("procedure p(a,b); ; call p."), "ERROR line 1: wrong number of arguments");
    CHECK_EQ_STR(run("procedure p; ; call p(1)."), "ERROR line 1: wrong number of arguments");
}
static void call_non_proc(void) {
    PL0Error e;
    CHECK_EQ_INT(pl0_compile("var a; call a.", &prog, &e), 1);
    CHECK_EQ_INT(e.code, E_NOT_PROC);
    CHECK_EQ_STR(run("const c=1; call c."), "ERROR line 1: not a procedure");
    CHECK_EQ_STR(run("call q."), "ERROR line 1: undeclared name");
    CHECK_EQ_STR(run("call 1."), "ERROR line 1: identifier expected");
}
static void proc_not_a_variable(void) {
    CHECK_EQ_STR(run("procedure p; ; p := 1."), "ERROR line 1: not a variable");
    CHECK_EQ_STR(run("procedure p; ; !p."), "ERROR line 1: not a variable");
    CHECK_EQ_STR(run("procedure p; p := 1; ."), "ERROR line 1: not a variable");
}
static void proc_redeclared(void) {
    CHECK_EQ_STR(run("procedure p(a, a); ; ."), "ERROR line 1: name already declared");
    CHECK_EQ_STR(run("procedure p(a); var a; ; ."), "ERROR line 1: name already declared");
    CHECK_EQ_STR(run("procedure p(a); const a=1; ; ."), "ERROR line 1: name already declared");
    CHECK_EQ_STR(run("procedure p; ; procedure p; ; ."), "ERROR line 1: name already declared");
    CHECK_EQ_STR(run("var p; procedure p; ; ."), "ERROR line 1: name already declared");
    /* a parameter may shadow an outer name, even the procedure's own */
    CHECK_EQ_STR(run("var a; procedure p(a, p); !a+p; begin a := 1; call p(2, 3); !a end."), "51");
}
static void proc_syntax_errors(void) {
    CHECK_EQ_STR(run("procedure ; ."), "ERROR line 1: identifier expected");
    CHECK_EQ_STR(run("procedure p ."), "ERROR line 1: \";\" expected");
    CHECK_EQ_STR(run("procedure p(a ; ."), "ERROR line 1: \")\" expected");
    CHECK_EQ_STR(run("procedure p(a,) ; ."), "ERROR line 1: identifier expected");
    CHECK_EQ_STR(run("procedure p; !1 ."), "ERROR line 1: \";\" expected");
    CHECK_EQ_STR(run("procedure p(a); ; call p(1 ."), "ERROR line 1: \")\" expected");
}
/* n procedures nested inside each other; the innermost prints 1. */
static const char *nested_procs(int n) {
    char *p = big;
    int i;
    for (i = 0; i < n; i++) p += sprintf(p, "procedure p%d; ", i);
    p += sprintf(p, "!1");
    for (i = n - 1; i >= 0; i--) p += sprintf(p, "; call p%d", i);
    strcpy(p, ".");
    return big;
}
static void nesting_too_deep(void) {
    PL0Error e;
    CHECK_EQ_STR(run(nested_procs(MAX_NEST)), "1");                  /* 4 levels: ok */
    CHECK_EQ_INT(pl0_compile(nested_procs(MAX_NEST + 1), &prog, &e), 1);
    CHECK_EQ_INT(e.code, E_NESTING);
    CHECK_EQ_STR(run(nested_procs(MAX_NEST + 1)), "ERROR line 1: nested too deeply");
}
static void nesting_counters_per_body(void) {
    /* a procedure body full of nested statements does not use up the main
       program's statement nesting budget, and vice versa */
    char *p = big;
    int i;
    p += sprintf(p, "procedure q; ");
    for (i = 0; i < MAX_STMT_NEST; i++) p += sprintf(p, "begin ");
    p += sprintf(p, "!1");
    for (i = 0; i < MAX_STMT_NEST; i++) p += sprintf(p, " end");
    p += sprintf(p, "; ");
    for (i = 0; i < MAX_STMT_NEST; i++) p += sprintf(p, "begin ");
    p += sprintf(p, "call q");
    for (i = 0; i < MAX_STMT_NEST; i++) p += sprintf(p, " end");
    strcpy(p, ".");
    CHECK_EQ_STR(run(big), "1");
}
static void runaway_recursion(void) {
    CHECK_EQ_STR(run("procedure p; call p; call p."), "ERROR line 1: stack overflow (too deep)");
    CHECK(vm_stack_high_water() <= MAX_STACK);
    CHECK(vm_stack_high_water() > MAX_STACK - 8);   /* it really filled the stack */
    /* the line is that of the call that did not fit */
    CHECK_EQ_STR(run("var n;\nprocedure p;\nbegin\n  n := n + 1;\n  call p\nend;\ncall p."),
                 "ERROR line 5: stack overflow (too deep)");
    CHECK(vm_stack_high_water() <= MAX_STACK);
    /* with parameters, locals and temporaries in each frame */
    CHECK_EQ_STR(run("procedure p(a, b);\nvar c, d;\nbegin c := a*(b+(a*(b+1))) - a*(b+(a*(b+1))); call p(c, d+1) end;\ncall p(1, 2)."),
                 "ERROR line 3: stack overflow (too deep)");
    CHECK(vm_stack_high_water() <= MAX_STACK);
    /* the VM is usable again after an overflow */
    CHECK_EQ_STR(run("procedure p(a); !a; call p(4)."), "4");
}
static void instr_count(void) {
    unsigned long a, b;
    /* the loop body "LOD LIT LT JPC LOD LIT ADD STO JMP" is 9 instructions */
    CHECK_EQ_STR(run("var i; while i < 10 do i := i + 1."), "");
    a = vm_instr_count();
    CHECK_EQ_STR(run("var i; while i < 11 do i := i + 1."), "");
    b = vm_instr_count();
    CHECK_EQ_INT((int)(b - a), 9);
    CHECK_EQ_INT((int)vm_fn_count(), 0);
    CHECK_EQ_STR(run("var i; while i < 3 do i := i + abs(int(1.5))."), "");
    CHECK_EQ_INT((int)vm_fn_count(), 6);
    /* reset per run; even the empty program executes INT and RET */
    CHECK_EQ_STR(run("."), "");
    CHECK(vm_instr_count() >= 2 && vm_instr_count() < 5);
}
static const char *sum_program(long n) {
    sprintf(big, "var r;\n"
                 "procedure sum(n);\n"
                 "begin\n"
                 "  if n = 0 then r := 0;\n"
                 "  if n > 0 then begin call sum(n-1); r := r+n end\n"
                 "end;\n"
                 "begin call sum(%ld); !r end.", n);
    return big;
}
static void recursion_depth(void) {
    CHECK_EQ_STR(run(sum_program(40)), "820");
    CHECK(vm_stack_high_water() <= MAX_STACK);
    CHECK_EQ_STR(run(sum_program(100000L)), "ERROR line 5: stack overflow (too deep)");
    CHECK(vm_stack_high_water() <= MAX_STACK);
}
static void stopped_in_procedure(void) {
    io_host_break_after(4);
    CHECK_EQ_STR(run("procedure p(x);\nbegin\n  while x > 0 do x := x\nend;\ncall p(1)."), "ERROR line 3: stopped");
    io_host_break_after(0);
}
static void runtime_error_in_procedure(void) {
    CHECK_EQ_STR(run("procedure p(x);\n  !1/x;\nbegin call p(2); call p(0) end."), "ERROR line 2: division by zero");
}
static void proc_after_vars_and_consts(void) {
    CHECK_EQ_STR(run("const k=10; var a; procedure p(x); var b; begin b := x*k; a := a+b end;"
                     " begin call p(1); call p(2); !a end."), "30");
    /* procedures declared at the same level: a later one may call an earlier one */
    CHECK_EQ_STR(run("procedure a(x); !x; procedure b(y); call a(y+1); call b(1)."), "2");
}

/* ---- math functions and input (Task 8) ---- */
static void sqrt_basic(void) { CHECK_EQ_STR(run("!sqrt(16)."), "4"); CHECK_EQ_STR(run("!sqrt(2)*sqrt(2)."), "2"); }
static void sin_cos(void) {
    CHECK_EQ_STR(run("!sin(0)."), "0");
    CHECK_EQ_STR(run("!cos(0)."), "1");
    CHECK_EQ_STR(run("!sin(1.5707963267949)."), "1");
}
static void atan_pi(void) { CHECK_EQ_STR(run("!4*atan(1)."), "3.14159265"); }
static void int_floor(void) {
    CHECK_EQ_STR(run("!int(2.7)."), "2");
    CHECK_EQ_STR(run("!int(-2.7)."), "-3");
    CHECK_EQ_STR(run("!int(-2)."), "-2");
}
static void abs_exp_log(void) {
    CHECK_EQ_STR(run("!abs(-3.5)."), "3.5");
    CHECK_EQ_STR(run("!abs(2)."), "2");
    CHECK_EQ_STR(run("!log(exp(1))."), "1");
    CHECK_EQ_STR(run("!exp(0)."), "1");
}
static void fn_composition(void) {
    CHECK_EQ_STR(run("!sqrt(abs(-16))+1."), "5");
    CHECK_EQ_STR(run("!1+sqrt(sqrt(16))*2."), "5");
    CHECK_EQ_STR(run("var x; begin x := 9; !sqrt(x)+sqrt(x*x) end."), "12");
    CHECK_EQ_STR(run("procedure p(a); !sqrt(a)*a; call p(4)."), "8");
    CHECK_EQ_STR(run("!-sqrt(4)."), "-2");
}
static void sqrt_negative(void) {
    CHECK_EQ_STR(run("!sqrt(0-1)."), "ERROR line 1: domain error");
    CHECK_EQ_STR(run("var a;\nbegin\n a := 1;\n !a+\nsqrt(0-a)\nend."), "ERROR line 5: domain error");
}
static void log_zero(void) {
    CHECK_EQ_STR(run("!log(0)."), "ERROR line 1: domain error");
    CHECK_EQ_STR(run("!log(0-2)."), "ERROR line 1: domain error");
}
static void exp_overflow(void) { CHECK_EQ_STR(run("!exp(1000)."), "ERROR line 1: number overflow"); }
static void div_by_zero(void) {
    CHECK_EQ_STR(run("!1/0."), "ERROR line 1: division by zero");
    CHECK_EQ_STR(run("!\n1\n/\n0."), "ERROR line 3: division by zero");
}
static void function_missing_paren(void) {
    CHECK_EQ_STR(run("!sqrt 16."), "ERROR line 1: \"(\" expected");
    CHECK_EQ_STR(run("!sqrt(16."), "ERROR line 1: \")\" expected");
    CHECK_EQ_STR(run("!sqrt()."), "ERROR line 1: expression expected");
    CHECK_EQ_STR(run("!sqrt."), "ERROR line 1: \"(\" expected");
}
static const char *fns(int n) {
    char *p = big;
    int i;
    *p++ = '!';
    for (i = 0; i < n; i++) { memcpy(p, "abs(", 4); p += 4; }
    *p++ = '1';
    for (i = 0; i < n; i++) *p++ = ')';
    strcpy(p, ".");
    return big;
}
static void function_nesting_limit(void) {
    CHECK_EQ_STR(run(fns(MAX_EXPR_NEST)), "1");
    CHECK_EQ_STR(run(fns(MAX_EXPR_NEST + 1)), "ERROR line 1: nested too deeply");
    /* functions and parentheses share the counter */
    {   /* abs followed by k "(": the function paren plus k-1 grouping parens = k levels */
        char src[64]; int k;
        for (k = MAX_EXPR_NEST; k <= MAX_EXPR_NEST + 1; k++) {
            char *q = src; int i;
            memcpy(q, "!abs", 4); q += 4;
            for (i = 0; i < k; i++) *q++ = '(';
            *q++ = '1';
            for (i = 0; i < k; i++) *q++ = ')';
            strcpy(q, ".");
            CHECK_EQ_STR(run(src), k > MAX_EXPR_NEST ? "ERROR line 1: nested too deeply" : "1");
        }
    }
}
static void input_ok(void) {
    io_host_set_input("7");
    CHECK_EQ_STR(run("var x; begin ?x; !x*2 end."), "14");
    io_host_set_input("  3.5\n-2  10 ");
    CHECK_EQ_STR(run("var a,b,c; begin ?a; ?b; ?c; !a; !b; !c end."), "3.5-210");
    io_host_set_input("5");
    CHECK_EQ_STR(run("var x;\nprocedure p; ?x;\nbegin call p; !x end."), "5");
}
static void input_bad(void) {
    io_host_set_input("abc");
    CHECK_EQ_STR(run("var x; ?x."), "ERROR line 1: bad input");
    io_host_set_input("");
    CHECK_EQ_STR(run("var x;\n?x."), "ERROR line 2: bad input");
    io_host_set_input("1");
    CHECK_EQ_STR(run("var x,y; begin ?x;\n?y end."), "ERROR line 2: bad input");  /* queue exhausted */
}
static void input_compile_errors(void) {
    CHECK_EQ_STR(run("const c=1; ?c."), "ERROR line 1: not a variable");
    CHECK_EQ_STR(run("procedure p; ; ?p."), "ERROR line 1: not a variable");
    CHECK_EQ_STR(run("?z."), "ERROR line 1: undeclared name");
    CHECK_EQ_STR(run("?."), "ERROR line 1: identifier expected");
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
    overflow_loop();
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
    proc_no_params();
    proc_params();
    factorial_recursive();
    fib_recursive();
    nested_proc_sees_outer_var();
    params_are_by_value();
    args_count_mismatch();
    call_non_proc();
    proc_not_a_variable();
    proc_redeclared();
    proc_syntax_errors();
    nesting_too_deep();
    nesting_counters_per_body();
    runaway_recursion();
    instr_count();
    recursion_depth();
    stopped_in_procedure();
    runtime_error_in_procedure();
    proc_after_vars_and_consts();
    sqrt_basic(); sin_cos(); atan_pi(); int_floor(); abs_exp_log(); fn_composition();
    sqrt_negative(); log_zero(); exp_overflow(); div_by_zero(); function_missing_paren();
    function_nesting_limit(); input_ok(); input_bad(); input_compile_errors();
    TEST_MAIN_END();
}
