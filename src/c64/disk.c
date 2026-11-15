/* disk.c -- program files on the C64 drive (disk.h): load, save and
 * the messages, built on the KERNAL primitives in disk.s (dk_open,
 * dk_text, dk_write, dk_cmd, dk_close; how they use the drive's channels
 * is described there).
 *
 * Load reads the file twice: the first pass only counts the text bytes
 * (stopping once there are too many), so a missing or too big file
 * leaves the buffer as it was; the second stores them at the start of the
 * buffer storage (the gap after them, cursor 0). Save writes the whole
 * text (tb_text) to NAME,S,W; the drive refuses an existing name with 63
 * FILE EXISTS before anything is written, which is how the caller learns
 * that it must ask; a replace scratches the old file first (S0:NAME),
 * which works the same on the 1541 and VICE's virtual drive and avoids
 * the 1541's save-with-replace (@0:) bug.
 *
 * Compiled with the ASCII charmap like every C64 source: the literals
 * below (",S,R", "S0:") are bytes the drive reads as PETSCII; ',', ':',
 * digits and upper case letters are the same in both. */
#include <string.h>
#include "core/ascii.h"
#include "c64/disk.h"

/* disk_message: "DISK ERROR: " + one of these; the buffer is checked at
 * compile time to hold the longest (a message once overran it) */
#define PREFIX  "DISK ERROR: "
#define T_NODISK "NO DISK"
#define T_NOTFOUND "FILE NOT FOUND"
#define T_EXISTS "FILE EXISTS"
#define T_FULL "DISK FULL"
#define T_READ "READ FAILED"
#define MSG_MAX 26              /* PREFIX + the longest text (FILE NOT FOUND) */
/* each array below has a size <= 0, a compile error, if a text is too long */
#define FITS(t) typedef char fits_##t[MSG_MAX + 3 - sizeof PREFIX - sizeof t]
FITS(T_NODISK);
FITS(T_NOTFOUND);
FITS(T_EXISTS);
FITS(T_FULL);
FITS(T_READ);
typedef char fits_number[MSG_MAX + 2 - sizeof "DISK ERROR 00"];
typedef char fits_toobig[MSG_MAX + 2 - sizeof "FILE TOO BIG"];

/* LOWBSS: the cassette buffer (pl0-c64.cfg), cleared by main */
#pragma bss-name (push, "LOWBSS")
uint8_t disk_touched;

static char fn[24];             /* file name or command for the drive */
static char mbuf[MSG_MAX + 1];  /* disk_message */
static uint8_t rc;
static uint16_t n;
#pragma bss-name (pop)

static void make_fn(const char *pre, const char *name, const char *suf)
{
    strcpy(fn, pre);
    strcat(fn, name);
    strcat(fn, suf);
}

uint8_t disk_load(const char *name, TextBuf *t)
{
    make_fn("", name, ",S,R");
    disk_touched = 0;
    if ((rc = dk_open(fn)) != 0) return rc;
    dk_lim = t->cap;            /* cap text bytes: too big */
    n = dk_text(0);
    if ((rc = dk_close()) != 0) return rc;
    if (n >= t->cap) return DK_TOOBIG;
    if ((rc = dk_open(fn)) != 0) return rc;
    disk_touched = 1;
    dk_lim = (uint16_t)(t->cap - 1);
    n = dk_text(t->buf);
    rc = dk_close();
    t->len = n;                 /* text at the start, the gap after it */
    t->gs = n;
    t->cur = 0;
    t->want = 0;
    return rc;
}

uint8_t disk_save(const char *name, TextBuf *t, uint8_t replace)
{
    const char *p = tb_text(t);
    if (replace) {
        make_fn("S0:", name, "");
        if ((rc = dk_cmd(fn)) != 0) return rc;
    }
    make_fn("", name, ",S,W");
    if ((rc = dk_open(fn)) != 0) return rc;
    dk_write(p);
    return dk_close();
}

const char *disk_message(uint8_t code)
{
    const char *s = 0;
    if (code == DK_TOOBIG) return "FILE TOO BIG";
    /* no drive; 74 DRIVE NOT READY; a 1541 without a disk also answers
     * 21 READ ERROR (no sync mark found) */
    if (code == DK_NODEV || code == 74 || code == 21) s = T_NODISK;
    else if (code == 62) s = T_NOTFOUND;
    else if (code == 63) s = T_EXISTS;
    else if (code == 72) s = T_FULL;
    else if (code == DK_READ) s = T_READ;
    if (s) {
        strcpy(mbuf, PREFIX);
        strcat(mbuf, s);
    } else {
        strcpy(mbuf, "DISK ERROR 00");
        mbuf[11] = (char)('0' + code / 10);
        mbuf[12] = (char)('0' + code % 10);
    }
    return mbuf;
}
