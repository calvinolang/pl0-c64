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
 *   OPR  sub-op in lvl, arg = source line (for errors)   arithmetic
 *        OPR_RET: arg = FRAME_ARG(parameter bytes to pop, block level)
 *   LOD  lvl = absolute declaration level, arg = byte offset in that frame
 *        (parameters sit below the frame header: negative offsets)
 *   STO  same as LOD; pops into the variable
 *   CAL  arg = CAL_ARG(code address, line), lvl = CAL_LVL(line); the source
 *        line of the call statement is packed into the bits the address does
 *        not use (address < 1024, line < 16384), read back with CAL_LINE.
 *        The target is always the callee's INT.
 *   INT  lvl = max temporaries the block pushes,
 *        arg = FRAME_ARG(bytes of local variables to reserve, block level)
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
 *   (parameters, pushed by the caller left to right, just below the base)
 *   FRAME_SL  saved display entry of the callee's level (static link role)
 *   FRAME_DL  dynamic link: caller's frame base
 *   FRAME_RA  return address
 * followed by locals (variables) and then expression temporaries.
 * The main program frame has the same header (unused) at the stack bottom.
 * CAL writes DL and RA; the callee's INT saves and sets the display entry
 * of its level; OPR_RET undoes both and pops the parameters. */
enum { FRAME_SL, FRAME_DL, FRAME_RA, FRAME_HDR };

/* INT and OPR_RET: a byte count (< 4096) plus the block's level (top 4 bits). */
#define FRAME_ARG(bytes, level) ((int16_t)((uint16_t)(bytes) | ((uint16_t)(level) << 12)))
#define FRAME_BYTES(arg)        ((uint16_t)((uint16_t)(arg) & 0x0FFFu))
#define FRAME_LEVEL(arg)        ((uint8_t)((uint16_t)(arg) >> 12))

/* CAL: code address in the low 10 bits of arg, line bits 8..13 above it,
 * line bits 0..7 in lvl. */
#define CAL_ARG(addr, line) ((int16_t)((uint16_t)(addr) | (((uint16_t)(line) >> 8) << 10)))
#define CAL_LVL(line)       ((uint8_t)((line) & 0xFFu))
#define CAL_ADDR(arg)       ((uint16_t)((uint16_t)(arg) & 0x03FFu))
#define CAL_LINE(in)        ((uint16_t)((in)->lvl | (((uint16_t)(in)->arg >> 10) << 8)))
#define CAL_MAX_LINE        0x3FFFu
#if MAX_CODE > 1024
#error "CAL packs the code address into 10 bits"
#endif

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
