/* disk_check.c -- the KERNAL disk primitives of src/c64/disk.c on the
 * emulated drive (make c64-disk; build/pl0.d64 attached as device 8).
 *
 * Prints one labelled line per step; tests/vice_disk.sh compares the
 * screen and then reads the written file back from the image with c1541:
 *   dev   the device number
 *   dir   directory lines from "$" (the text from the first quote): the
 *         header, the first two files, the last file, the line count
 *   idx   the first line of INDEX
 *   s1    save DISKTEST (new file): 0
 *   s2    save DISKTEST again without replace: 63 (FILE EXISTS)
 *   s3    save DISKTEST with replace: 0 (scratch, then write)
 *   l1    load DISKTEST: 0 and the text, which must equal what was saved
 *   l2    load NOPE: 62 (FILE NOT FOUND)
 *   l3    load FACTORIAL into a 100-byte buffer: 253 (too big), the buffer
 *         unchanged
 * then DONE. The text saved lives in the source buffer under the BASIC
 * ROM ($A000), as in the program. */
#include <string.h>
#include <stdint.h>
#include "core/ascii.h"
#include "core/limits.h"
#include "c64/disk.h"
#include "c64/textbuf.h"
#include "platform/io_c64.h"
#include "out.h"

#pragma bss-name (push, "BASICBSS")
static char source[MAX_SOURCE];
#pragma bss-name (pop)
static char small[100];
static TextBuf t, ts;
static char line[48], last[48];
static uint8_t r;

static void code(const char *label, uint8_t c)
{
    out_str(label);
    out_str(" ");
    out_ul(c);
    out_nl();
}

/* one directory entry per line: the text of a listing line (after the
 * link and the block count) up to its end */
static void dir(void)
{
    int k;
    uint8_t n, i, q;
    r = dk_open("$");
    code("dir open", r);
    if (r) return;
    dk_getc(); dk_getc();               /* load address */
    for (n = 0; n < 30; ++n) {
        if (dk_getc() < 0) break;       /* link */
        dk_getc();
        dk_getc(); dk_getc();           /* blocks */
        i = 0; q = 0;
        while ((k = dk_getc()) > 0) {
            if (k == '"') ++q;
            if (q && i < 40) line[i++] = (char)k;
        }
        line[i] = 0;
        if (k < 0) break;
        if (n < 3) {                    /* the header, PL0, INDEX */
            out_str("dir ");
            out_line(line);
        }
        if (i) memcpy(last, line, sizeof line);
    }
    out_str("dir last ");
    out_line(last);
    code("dir lines", n);
    code("dir close", dk_close());
}

int main(void)
{
    int k;
    uint8_t i;
    const char *txt = "{ disk test }\n!1+2.\n";
    io_text_open();
    disk_init();
    code("dev", disk_dev);
    dir();
    r = dk_open("INDEX,S,R");
    i = 0;
    while (r == 0 && (k = dk_getc()) >= 0 && k != '\n' && i < 40) line[i++] = (char)k;
    line[i] = 0;
    out_str("idx ");
    out_line(line);
    code("idx close", dk_close());

    tb_init(&t, source, sizeof source);
    tb_set(&t, txt);
    code("s1", disk_save("DISKTEST", &t, 0));
    code("s2", disk_save("DISKTEST", &t, 0));
    code("s3", disk_save("DISKTEST", &t, 1));
    tb_set(&t, "x");
    code("l1", disk_load("DISKTEST", &t));
    if (strcmp(tb_text(&t), txt) == 0) out_line("l1 same");
    else out_line("l1 DIFFERENT");
    code("l2", disk_load("NOPE", &t));
    tb_init(&ts, small, sizeof small);
    tb_set(&ts, "keep");
    code("l3", disk_load("FACTORIAL", &ts));
    if (strcmp(tb_text(&ts), "keep") == 0) out_line("l3 kept");
    else out_line("l3 CHANGED");
    out_line("DONE");
    for (;;) ;                  /* returning to BASIC would print READY. */
    return 0;
}
