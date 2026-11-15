/* weight.h -- SPIKE weight stub shared declarations. Limits can be
 * overridden with -D on the command line (see ../Makefile). */
#ifndef WEIGHT_H
#define WEIGHT_H
#include <stdint.h>
#include "platform/num.h"   /* the real interface from src/ (read-only use) */

#ifndef MAX_CODE
#define MAX_CODE   2048     /* P-code instructions, 4 bytes each */
#endif
#ifndef MAX_IDENTS
#define MAX_IDENTS 64
#endif
#ifndef MAX_STACK
#define MAX_STACK  1024     /* runtime stack, 5-byte numbers */
#endif
#ifndef MAX_SOURCE
#define MAX_SOURCE 16384
#endif
#ifndef MAX_CONSTS
#define MAX_CONSTS 64       /* literal pool, 5-byte numbers */
#endif
#ifndef MAX_STRINGS
#define MAX_STRINGS 512     /* bytes of '...' text for ! statements */
#endif
#define MAX_NEST   4
#define MAX_NAME   16

typedef struct { uint8_t op, lev; uint16_t a; } Instr;          /* 4 bytes */
typedef struct { char name[MAX_NAME + 1]; uint8_t kind, level, nparams;
                 uint16_t addr; } Ident;                         /* 22 bytes */

extern Instr code[MAX_CODE];
extern num_t stack[MAX_STACK];
extern num_t consts[MAX_CONSTS];
extern char  strings[MAX_STRINGS];
extern Ident idents[MAX_IDENTS];
extern char  source[MAX_SOURCE];

enum { OP_LIT, OP_LOD, OP_STO, OP_CAL, OP_INT, OP_JMP, OP_JPC, OP_RET,
       OP_NEG, OP_ADD, OP_SUB, OP_MUL, OP_DIV, OP_ODD, OP_EQ, OP_NE,
       OP_LT, OP_LE, OP_GT, OP_GE, OP_FN, OP_WRT, OP_WRS, OP_WRL, OP_RD,
       OP_FWD, OP_TRN, OP_PU, OP_PD, OP_HOME, OP_MOVETO, OP_HLT };

uint8_t vm_run(void);          /* returns error code, 0 = ok */
uint8_t editor_run(void);

/* fp.s shims */
void __fastcall__ fp_load(const num_t *x);
unsigned char __fastcall__ fp_store(num_t *r);
unsigned char __fastcall__ fp_add(const num_t *x);
unsigned char __fastcall__ fp_sub(const num_t *x);
unsigned char __fastcall__ fp_mul(const num_t *x);
unsigned char __fastcall__ fp_div(const num_t *x);
unsigned char __fastcall__ fp_fn(unsigned char fn);
const char *fp_fout(void);
void __fastcall__ fp_from_int(int v);
int fp_to_int(void);
unsigned char __fastcall__ fp_parse(const char *s);
extern unsigned char fp_lasterr;
extern unsigned char fp_instrument;

/* platform hooks the VM needs (stubs) */
void io_put_str(const char *s);
void io_put_nl(void);
int  io_read_num(num_t *r);
int  io_break(void);
void turtle_forward(const num_t *d);
void turtle_turn(const num_t *d);
void turtle_pen(uint8_t down);
void turtle_home(void);
void turtle_moveto(const num_t *x, const num_t *y);
#endif
