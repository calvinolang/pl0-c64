#ifndef PL0_NUM_H
#define PL0_NUM_H
#include <stdint.h>

/* Platform number interface. All operations take pointers so a 5-byte
 * float type on the C64 can be passed without struct-by-value cost.
 * The representation is chosen here: cc65 builds get an opaque 5-byte
 * value (implemented in src/c64/num_c64.c and num_c64.s); everything else
 * (host builds and tests) uses double. Callers must treat num_t as opaque.
 * The result pointer r may alias an operand (the VM computes in place). */
#ifdef __CC65__
typedef struct { uint8_t b[5]; } num_t;
#else
typedef double num_t;
#endif

enum { NUMFN_SIN, NUMFN_COS, NUMFN_ATN, NUMFN_SQR, NUMFN_ABS, NUMFN_INT, NUMFN_EXP, NUMFN_LOG };

void num_copy(num_t *dst, const num_t *src);     /* never struct-assign num_t */
void num_from_int(num_t *r, int16_t v);
int  num_parse(const char *s, num_t *r);           /* 0 ok, -1 bad */
/* add/sub/mul/neg: 0 ok, nonzero if the result overflowed or is not finite
 * (the C64 ROM traps overflow, so this is portable). r may alias an operand;
 * r is unspecified after an error. */
int  num_add(num_t *r, const num_t *a, const num_t *b);
int  num_sub(num_t *r, const num_t *a, const num_t *b);
int  num_mul(num_t *r, const num_t *a, const num_t *b);
int  num_div(num_t *r, const num_t *a, const num_t *b);   /* 0 ok, 1 b==0, 2 overflow */
int  num_neg(num_t *r, const num_t *a);
int  num_cmp(const num_t *a, const num_t *b);      /* <0, 0, >0 */
int  num_is_zero(const num_t *a);
int  num_is_odd(const num_t *a);                   /* odd integer part (floor) */
int16_t num_to_i16(const num_t *a);                /* floor, saturating */
int  num_fn(uint8_t fn, num_t *r, const num_t *a); /* 0 ok, 1 domain error, 2 overflow; r may alias a */
void num_format(const num_t *a, char *buf);        /* buf >= 16 bytes */

#endif
