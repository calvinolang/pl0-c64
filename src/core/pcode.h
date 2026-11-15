#ifndef PL0_PCODE_H
#define PL0_PCODE_H
#include <stdint.h>
#include "core/limits.h"
#include "platform/num.h"

/* P-code: Wirth's PL/0 instruction set (LIT OPR LOD STO CAL INT JMP JPC)
 * plus output/input opcodes. Opcode values are defined here and only here.
 *
 * Operand conventions (all offsets are BYTE offsets, pre-scaled by
 * sizeof(num_t), so the VM never multiplies by 5 on the C64):
 *   LIT  -,     arg = byte offset into consts           push consts
 *   OPR  sub-op in lvl, arg = source line (for errors)   arithmetic / RET
 *   LOD  lvl = absolute declaration level, arg = byte offset in that frame
 *   STO  same as LOD; pops into the variable
 *   CAL  lvl = level of the callee's body, arg = code address   (Task 7)
 *   INT  lvl = max temporaries the block pushes, arg = bytes to reserve
 *   JMP  arg = code address; backward jumps poll io_break()     (Task 6)
 *   JPC  arg = code address; pop, jump if zero                  (Task 6)
 *   WRN  pop and print a number
 *   WRS  arg = byte offset into strings (NUL-terminated)
 *   WRL  print a newline
 *   RED  arg = source line; push a number read from input       (Task 8)
 */
enum { OP_LIT, OP_OPR, OP_LOD, OP_STO, OP_CAL, OP_INT, OP_JMP, OP_JPC,
       OP_WRN, OP_WRS, OP_WRL, OP_RED };

/* OPR sub-operations (stored in Instr.lvl), Wirth's numbering. */
enum { OPR_RET, OPR_NEG, OPR_ADD, OPR_SUB, OPR_MUL, OPR_DIV, OPR_ODD,
       OPR_EQ = 8, OPR_NE, OPR_LT, OPR_GE, OPR_GT, OPR_LE };

/* Activation record (frame) layout, in cells from the frame base:
 *   FRAME_SL  saved display entry of the callee's level (static link role)
 *   FRAME_DL  dynamic link: caller's frame base
 *   FRAME_RA  return address
 * followed by locals (variables) and then expression temporaries.
 * The main program frame has the same header (unused) at the stack bottom. */
enum { FRAME_SL, FRAME_DL, FRAME_RA, FRAME_HDR };

typedef struct { uint8_t op; uint8_t lvl; int16_t arg; } Instr;

typedef struct {
    Instr    code[MAX_CODE];
    uint16_t ncode;
    num_t    consts[MAX_CONSTS];
    uint8_t  nconsts;
    char     strings[MAX_STRINGS];
    uint16_t nstrings;          /* bytes used in strings */
} Program;

#endif
