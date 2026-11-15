/* One-pass recursive-descent PL/0 compiler emitting P-code (after Wirth).
 *
 * C64 notes: all state is static (cc65 accesses statics faster than stack
 * locals and its software stack is small); errors unwind with longjmp so the
 * compiler stops at the first error without checks after every call.
 * Recursion depth is bounded by MAX_EXPR_NEST (expressions), MAX_STMT_NEST
 * (structured statements) and, from Task 7, MAX_NEST (procedures); all three
 * report E_NESTING. */
#include <setjmp.h>
#include <string.h>
#include "core/compiler.h"
#include "core/lexer.h"

#define CELL ((int16_t)sizeof(num_t))

enum { K_CONST, K_VAR, K_PROC };

/* val: K_CONST byte offset into consts, K_VAR byte offset in its frame
 * (negative for parameters), K_PROC code address of its INT.
 * level: block level of the declaration (a procedure's body is level + 1).
 * nparams: K_PROC only. */
typedef struct { char name[17]; uint8_t kind; uint8_t level; uint8_t nparams; int16_t val; } Sym;

static Lexer lx;
static Program *prog;
static PL0Error *perr;
static jmp_buf on_error;
static Sym syms[MAX_IDENTS];
static uint8_t nsyms;
static uint8_t level;      /* absolute declaration level of the current block */
static uint8_t depth;      /* VM temporaries currently on the stack */
static uint8_t maxdepth;   /* maximum of depth in the current block */
static uint8_t nest;       /* current expression nesting: ( and unary signs */
static uint8_t snest;      /* current structured-statement nesting */

static void expression(void);
static void block(uint8_t first, uint8_t nparams);

static void fail(uint8_t code)
{
    perr->code = code;
    perr->line = lx.line;
    longjmp(on_error, 1);
}

static void next(void)
{
    lex_next(&lx);
    if (lx.tok == T_ERROR) fail(lx.err);
}

static void expect(Token t, uint8_t code)
{
    if (lx.tok != t) fail(code);
    next();
}

static uint16_t emit(uint8_t op, uint8_t lvl, int16_t arg)
{
    Instr *in;
    if (prog->ncode >= MAX_CODE) fail(E_PROGRAM_TOO_BIG);
    in = &prog->code[prog->ncode];
    in->op = op;
    in->lvl = lvl;
    in->arg = arg;
    return prog->ncode++;
}

/* Track the VM stack depth so INT can reserve room for temporaries. */
static void push(void)
{
    if (++depth > maxdepth) maxdepth = depth;
}

/* Literal pool, deduplicated. memcmp (not num_cmp) keeps this cheap on the
 * C64, where num_cmp is a ROM call. Returns the byte offset. */
static int16_t add_const(const num_t *v)
{
    uint8_t i;
    for (i = 0; i < prog->nconsts; i++)
        if (memcmp(&prog->consts[i], v, sizeof(num_t)) == 0) return (int16_t)(i * CELL);
    if (prog->nconsts >= MAX_CONSTS) fail(E_PROGRAM_TOO_BIG);
    num_copy(&prog->consts[prog->nconsts], v);
    return (int16_t)(prog->nconsts++ * CELL);
}

static int16_t add_string(const char *s)
{
    uint16_t at = prog->nstrings;
    uint16_t n = (uint16_t)strlen(s) + 1;
    if (n > MAX_STRINGS - at) fail(E_PROGRAM_TOO_BIG);
    memcpy(prog->strings + at, s, n);
    prog->nstrings = at + n;
    return (int16_t)at;
}

/* Innermost declaration of the current identifier, or 0. */
static Sym *find(void)
{
    uint8_t i = nsyms;
    while (i) {
        --i;
        if (strcmp(syms[i].name, lx.text) == 0) return &syms[i];
    }
    return 0;
}

/* Declare the current identifier in the block whose names start at first. */
static Sym *declare(uint8_t kind, uint8_t first)
{
    Sym *s;
    uint8_t i;
    if (lx.tok != T_IDENT) fail(E_EXPECTED_IDENT);
    for (i = first; i < nsyms; i++)
        if (strcmp(syms[i].name, lx.text) == 0) fail(E_REDECLARED);
    if (nsyms >= MAX_IDENTS) fail(E_TOO_MANY_IDENTS);
    s = &syms[nsyms++];
    strcpy(s->name, lx.text);
    s->kind = kind;
    s->level = level;
    s->nparams = 0;
    s->val = 0;
    return s;
}

/* factor = ident | number | "(" expression ")" | ("-" | "+") factor */
static void factor(void)
{
    Sym *s;
    uint16_t line;
    uint8_t neg;

    switch (lx.tok) {
    case T_IDENT:
        s = find();
        if (!s) fail(E_UNDECLARED);
        if (s->kind == K_CONST) emit(OP_LIT, 0, s->val);
        else if (s->kind == K_VAR) emit(OP_LOD, s->level, s->val);
        else fail(E_NOT_VAR);
        push();
        next();
        break;
    case T_NUMBER:
        emit(OP_LIT, 0, add_const(&lx.value));
        push();
        next();
        break;
    case T_LPAREN:
        if (++nest > MAX_EXPR_NEST) fail(E_NESTING);
        next();
        expression();
        expect(T_RPAREN, E_EXPECTED_RPAREN);
        --nest;
        break;
    case T_MINUS:
    case T_PLUS:
        if (++nest > MAX_EXPR_NEST) fail(E_NESTING);
        line = lx.line;
        neg = lx.tok == T_MINUS;
        next();
        factor();
        if (neg) emit(OP_OPR, OPR_NEG, (int16_t)line);
        --nest;
        break;
    default:
        fail(E_EXPECTED_EXPR);
    }
}

/* term = factor { ("*" | "/") factor } */
static void term(void)
{
    uint8_t op;
    uint16_t line;
    factor();
    while (lx.tok == T_STAR || lx.tok == T_SLASH) {
        op = lx.tok == T_STAR ? OPR_MUL : OPR_DIV;
        line = lx.line;            /* runtime errors report the operator's line */
        next();
        factor();
        emit(OP_OPR, op, (int16_t)line);
        --depth;
    }
}

/* expression = term { ("+" | "-") term }   (signs are handled in factor) */
static void expression(void)
{
    uint8_t op;
    uint16_t line;
    term();
    while (lx.tok == T_PLUS || lx.tok == T_MINUS) {
        op = lx.tok == T_PLUS ? OPR_ADD : OPR_SUB;
        line = lx.line;
        next();
        term();
        emit(OP_OPR, op, (int16_t)line);
        --depth;
    }
}

/* Tokens that can begin a statement: used to say '";" expected' instead of
 * '"end" expected' when two statements are not separated. */
static int starts_statement(Token t)
{
    switch (t) {
    case T_IDENT: case T_BEGIN: case T_BANG: case T_QUERY: case T_IF:
    case T_WHILE: case T_CALL: case T_FORWARD: case T_TURN: case T_PENUP:
    case T_PENDOWN: case T_HOME: case T_MOVETO:
        return 1;
    default:
        return 0;
    }
}

/* condition = "odd" expression | expression ("=" | "#" | "<>" | "<" | "<=" |
 *             ">" | ">=") expression
 * Leaves one temporary (the truth value) for JPC to pop. The comparison OPR
 * is always the instruction just before the JPC and carries its line (used
 * by the VM to report E_STOPPED for a loop). */
static void condition(void)
{
    uint8_t op;
    uint16_t line;

    if (lx.tok == T_ODD) {
        line = lx.line;
        next();
        expression();
        emit(OP_OPR, OPR_ODD, (int16_t)line);
        return;
    }
    expression();
    switch (lx.tok) {
    case T_EQ: op = OPR_EQ; break;
    case T_NE: op = OPR_NE; break;
    case T_LT: op = OPR_LT; break;
    case T_LE: op = OPR_LE; break;
    case T_GT: op = OPR_GT; break;
    case T_GE: op = OPR_GE; break;
    default:   fail(E_EXPECTED_EXPR); return;   /* no relational operator */
    }
    line = lx.line;
    next();
    expression();
    emit(OP_OPR, op, (int16_t)line);
    --depth;
}

static void statement(void)
{
    Sym *s;
    uint16_t at, top, line;
    uint8_t n;
    switch (lx.tok) {
    case T_IDENT:
        s = find();
        if (!s) fail(E_UNDECLARED);
        if (s->kind != K_VAR) fail(E_NOT_VAR);
        next();
        expect(T_ASSIGN, E_EXPECTED_ASSIGN);
        expression();
        emit(OP_STO, s->level, s->val);
        --depth;
        break;
    case T_BEGIN:
        if (++snest > MAX_STMT_NEST) fail(E_NESTING);
        next();
        statement();
        for (;;) {
            if (lx.tok == T_SEMI) next();
            else if (starts_statement(lx.tok)) fail(E_EXPECTED_SEMI);
            else break;
            statement();
        }
        expect(T_END, E_EXPECTED_END);
        --snest;
        break;
    case T_IF:
        if (++snest > MAX_STMT_NEST) fail(E_NESTING);
        next();
        condition();
        expect(T_THEN, E_EXPECTED_THEN);
        at = emit(OP_JPC, 0, 0);
        --depth;
        statement();
        prog->code[at].arg = (int16_t)prog->ncode;
        --snest;
        break;
    case T_WHILE:
        if (++snest > MAX_STMT_NEST) fail(E_NESTING);
        top = prog->ncode;
        next();
        condition();
        expect(T_DO, E_EXPECTED_DO);
        at = emit(OP_JPC, 0, 0);
        --depth;
        statement();
        emit(OP_JMP, 0, (int16_t)top);       /* backward: VM polls io_break() */
        prog->code[at].arg = (int16_t)prog->ncode;
        --snest;
        break;
    case T_CALL:
        /* call ident ["(" expression {"," expression} ")"]: the arguments
         * are pushed left to right and become the callee's parameters. */
        line = lx.line;
        next();
        if (lx.tok != T_IDENT) fail(E_EXPECTED_IDENT);
        s = find();
        if (!s) fail(E_UNDECLARED);
        if (s->kind != K_PROC) fail(E_NOT_PROC);
        next();
        n = 0;
        if (lx.tok == T_LPAREN) {
            do {
                next();
                expression();
                if (++n > s->nparams) fail(E_ARGS);
            } while (lx.tok == T_COMMA);
            expect(T_RPAREN, E_EXPECTED_RPAREN);
        }
        if (n != s->nparams) fail(E_ARGS);
        if (line > CAL_MAX_LINE) line = CAL_MAX_LINE;
        emit(OP_CAL, CAL_LVL(line), CAL_ARG(s->val, line));
        depth -= n;                       /* RET pops the parameters */
        break;
    case T_BANG:
        next();
        if (lx.tok == T_STRING) {
            emit(OP_WRS, 0, add_string(lx.text));
            next();
        } else if (lx.tok == T_SEMI || lx.tok == T_END || lx.tok == T_PERIOD ||
                   lx.tok == T_EOF) {
            emit(OP_WRL, 0, 0);           /* bare "!" prints a newline */
        } else {
            expression();
            emit(OP_WRN, 0, 0);
            --depth;
        }
        break;
    default:
        break;                            /* empty statement */
    }
}

/* const ident = [-|+] number */
static void const_decl(uint8_t first)
{
    Sym *s;
    uint8_t neg = 0;
    num_t v;

    s = declare(K_CONST, first);
    next();
    expect(T_EQ, E_EXPECTED_EQ);
    if (lx.tok == T_MINUS || lx.tok == T_PLUS) {
        neg = lx.tok == T_MINUS;
        next();
    }
    if (lx.tok != T_NUMBER) fail(E_EXPECTED_EXPR);
    if (neg) num_neg(&v, &lx.value);
    else num_copy(&v, &lx.value);
    s->val = add_const(&v);
    next();
}

/* procedure ident ["(" ident {"," ident} ")"] ";" block ";"
 * Declared in the scope starting at first. The parameters are the first
 * names of the procedure's own scope, so they clash with each other and with
 * its locals (E_REDECLARED), and may shadow outer names. */
static void proc_decl(uint8_t first)
{
    Sym *s;
    uint8_t pfirst, n, i;

    next();
    s = declare(K_PROC, first);
    s->val = (int16_t)prog->ncode;       /* its block starts with its INT */
    next();
    if (++level > MAX_NEST) fail(E_NESTING);
    pfirst = nsyms;
    if (lx.tok == T_LPAREN) {
        do {
            next();
            declare(K_VAR, pfirst);
            next();
        } while (lx.tok == T_COMMA);
        expect(T_RPAREN, E_EXPECTED_RPAREN);
    }
    /* parameters sit just below the frame header: a, b -> -2, -1 cells */
    n = (uint8_t)(nsyms - pfirst);
    for (i = 0; i < n; i++) syms[pfirst + i].val = (int16_t)(((int16_t)i - n) * CELL);
    s->nparams = n;
    expect(T_SEMI, E_EXPECTED_SEMI);
    block(pfirst, n);
    --level;
    expect(T_SEMI, E_EXPECTED_SEMI);
}

/* block = [const ...;] [var ...;] {procedure ...;} statement
 * Code: INT, then (only if procedures follow) a JMP over their code, the
 * procedures, and the body. The INT comes first so a procedure's address is
 * known before its nested procedures (which may call it) are compiled.
 * first: index of the block's first name (its parameters, if any).
 * maxdepth/depth belong to the body; nested procedures overwrite them, so
 * they are reset right before the body (snest and nest are 0 at every
 * declaration, since declarations never occur inside a statement). */
static void block(uint8_t first, uint8_t nparams)
{
    int16_t off = FRAME_HDR * CELL;      /* next variable's byte offset */
    uint16_t at, jmp;
    Sym *s;

    if (lx.tok == T_CONST) {
        do { next(); const_decl(first); } while (lx.tok == T_COMMA);
        expect(T_SEMI, E_EXPECTED_SEMI);
    }
    if (lx.tok == T_VAR) {
        do {
            next();
            s = declare(K_VAR, first);
            s->val = off;
            off += CELL;
            next();
        } while (lx.tok == T_COMMA);
        expect(T_SEMI, E_EXPECTED_SEMI);
    }
    at = emit(OP_INT, 0, FRAME_ARG(off - FRAME_HDR * CELL, level));
    if (lx.tok == T_PROCEDURE) {
        jmp = emit(OP_JMP, 0, 0);
        do proc_decl(first); while (lx.tok == T_PROCEDURE);
        prog->code[jmp].arg = (int16_t)prog->ncode;
    }
    maxdepth = 0;
    depth = 0;
    statement();
    prog->code[at].lvl = maxdepth;       /* temporaries to check room for */
    emit(OP_OPR, OPR_RET, FRAME_ARG(nparams * CELL, level));
    nsyms = first;
}

int pl0_compile(const char *src, Program *p, PL0Error *err)
{
    prog = p;
    perr = err;
    p->ncode = 0;
    p->nconsts = 0;
    p->nstrings = 0;
    nsyms = 0;
    level = 0;
    depth = 0;
    nest = 0;
    snest = 0;
    err->code = E_NONE;
    err->line = 0;
    lex_init(&lx, src);
    if (setjmp(on_error)) return 1;
    next();
    block(0, 0);
    if (lx.tok != T_PERIOD) fail(E_EXPECTED_PERIOD);
    return 0;
}
