/* Text output helpers for the C64 check programs (out.h). */
#include "out.h"
#include "platform/io.h"

static char buf[12];

void out_str(const char *s) { io_put_str(s); }
void out_nl(void) { io_put_nl(); }
void out_line(const char *s) { io_put_str(s); io_put_nl(); }

void out_ul(unsigned long v)
{
    char *p = buf + sizeof buf - 1;
    *p = 0;
    do {
        *--p = (char)('0' + (uint8_t)(v % 10));
        v /= 10;
    } while (v);
    io_put_str(p);
}

void out_l(long v)
{
    if (v < 0) {
        io_put_str("-");
        out_ul(0UL - (unsigned long)v);
    } else {
        out_ul((unsigned long)v);
    }
}

void out_hex(uint8_t b)
{
    static const char hex[] = "0123456789abcdef";
    buf[0] = hex[b >> 4];
    buf[1] = hex[b & 15];
    buf[2] = 0;
    io_put_str(buf);
}
