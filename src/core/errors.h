#ifndef PL0_ERRORS_H
#define PL0_ERRORS_H
#include <stdint.h>

/* Minimal table; Task 4 appends the remaining codes after E_IDENT_LONG. */
enum { E_NONE = 0, E_COMMENT, E_STRING, E_CHAR, E_IDENT_LONG };

typedef struct { uint8_t code; uint16_t line; } PL0Error;

#endif
