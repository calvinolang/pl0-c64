/* cc65 2.18 -O miscompile: `cc65 -t c64 -O tests/c64/cc65_bug_sub.c -o /tmp/t.s`; WRONG output: `(int16_t)i - n` with i, n unsigned char is computed as `lda i / sec / sbc n` with X = 0 (no borrow into the high byte), so 0 - 1 gives 255 instead of -1; without -O it is right (tossubax), and with both operands cast, `(int16_t)i - (int16_t)n`, it is right too (tossuba0). Found in compiler.c (parameter offsets: every procedure parameter read 0 on the C64). Not built by any target. */
#include <stdint.h>
int16_t out;

void f(uint8_t i, uint8_t n)
{
    out = (int16_t)(((int16_t)i - n) * 5);     /* the miscompiled line */
}
