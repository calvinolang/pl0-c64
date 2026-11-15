/* disk.c -- SEQ files on the C64 drive through the KERNAL (disk.h).
 *
 * KERNAL calls through cc65's thin cbm_k_* wrappers (SETLFS, SETNAM,
 * OPEN, CHKIN, CHKOUT, BASIN, BSOUT, READST, CLRCH, CLOSE), no cc65 stdio
 * or cbm_open (larger). Logical file 15 is the drive's command/error
 * channel, opened without a name first (that never touches the bus); file
 * 2 is the data file. The status after every OPEN and after writing comes
 * from channel 15: `NN, TEXT,TT,SS` + CR, of which only NN is kept; 0-19
 * are fine (01 FILES SCRATCHED), and so is 73 (the drive's power-on
 * message, if nothing ran before). A missing drive shows as a failed OPEN
 * or CHKIN/CHKOUT (KERNAL error 5, device not present): DK_NODEV. Every
 * operation starts by closing 2 and 15, so a failed earlier one never
 * leaves a "file open" behind. Closing 15 closes every channel of the
 * drive, so the data file is always closed first.
 *
 * The text buffer is under the BASIC ROM ($A000): cc65 runs with the
 * BASIC ROM banked out, so plain C reads and writes reach that RAM; the
 * bytes pass to and from the KERNAL one at a time in A (never a pointer
 * into $A000 handed to a ROM routine).
 *
 * Compiled with the ASCII charmap like every C64 source: the literals
 * below ("$", ",S,R", "S0:") are bytes the drive reads as PETSCII; '$',
 * ',', ':', digits and upper case letters are the same in both. */
#include <string.h>
#include <cbm.h>
#include "core/ascii.h"
#include "c64/disk.h"

#define REG(a) (*(volatile uint8_t *)(a))
#define LF_DATA 2
#define LF_CMD  15

uint8_t disk_dev;

static char fn[24];             /* file name or command for the drive */
static char mbuf[24];           /* disk_message */
static uint8_t st, c, rc, eof, prevcr;
static uint16_t n, i, lim;
static char *p;

void disk_init(void)
{
    disk_dev = REG(0xBA);
    if (disk_dev < 8 || disk_dev > 30) disk_dev = 8;
}

static void close_all(void)
{
    cbm_k_clrch();
    cbm_k_close(LF_DATA);
    cbm_k_close(LF_CMD);
}

/* close everything, open the command channel (no bus traffic yet) */
static void begin(void)
{
    close_all();
    cbm_k_setlfs(LF_CMD, disk_dev, 15);
    cbm_k_setnam("");
    cbm_k_open();
}

/* the error channel's number: 0 when fine, DK_NODEV without a drive */
static uint8_t status(void)
{
    if (cbm_k_chkin(LF_CMD)) {
        cbm_k_clrch();
        return DK_NODEV;
    }
    rc = 0;
    for (i = 0; i < 40; ++i) {
        c = cbm_k_basin();
        st = cbm_k_readst();
        if (i < 2) {
            if (c < '0' || c > '9') {   /* no number: no answer */
                rc = DK_NODEV;
                break;
            }
            rc = (uint8_t)(rc * 10 + (c - '0'));
        }
        if (c == 13 || st) break;
    }
    cbm_k_clrch();
    if (rc < 20 || rc == 73) rc = 0;
    return rc;
}

static void make_fn(const char *pre, const char *name, const char *suf)
{
    strcpy(fn, pre);
    strcat(fn, name);
    strcat(fn, suf);
}

/* open fn as file 2 on secondary address sa; the drive's answer */
static uint8_t open_data(uint8_t sa)
{
    cbm_k_setlfs(LF_DATA, disk_dev, sa);
    cbm_k_setnam(fn);
    if (cbm_k_open()) return DK_NODEV;
    return status();
}

uint8_t dk_open(const char *fname, uint8_t sa)
{
    begin();
    if (fname != fn) strcpy(fn, fname);
    rc = open_data(sa);
    if (rc == 0 && cbm_k_chkin(LF_DATA)) rc = DK_NODEV;
    if (rc) close_all();
    eof = 0;
    return rc;
}

int dk_getc(void)
{
    if (eof) return -1;
    c = cbm_k_basin();
    st = cbm_k_readst();
    if (st & 0xBF) {            /* timeout or error: this byte is not data */
        eof = 1;
        return -1;
    }
    if (st) eof = 1;            /* $40 end of file: c is the last byte */
    return c;
}

uint8_t dk_close(void)
{
    cbm_k_clrch();
    cbm_k_close(LF_DATA);
    rc = status();
    close_all();
    return rc;
}

/* a file byte as text: '\n', a printable character, or 0 (dropped) */
static uint8_t conv(uint8_t k)
{
    if (k == 10 && prevcr) {    /* the LF of CR LF */
        prevcr = 0;
        return 0;
    }
    prevcr = 0;
    if (k == 13) prevcr = 1;
    if (k == 13 || k == 10) return '\n';
    if (k == 9) return ' ';
    if (k < 0x20 || k > 0x7E) return 0;
    return k;
}

/* one pass over the file fn: count the text bytes, and store them at p;
 * with p 0 only count, and stop once there are lim */
static uint8_t pass(void)
{
    static int k;
    rc = dk_open(fn, 2);
    if (rc) return rc;
    n = 0;
    prevcr = 0;
    while ((k = dk_getc()) >= 0) {
        c = conv((uint8_t)k);
        if (c) {
            if (p) *p++ = (char)c;
            ++n;
            if (p == 0 && n >= lim) break;      /* too big: enough */
        }
    }
    return dk_close();
}

uint8_t disk_load(const char *name, TextBuf *t)
{
    make_fn("", name, ",S,R");
    p = 0;
    lim = t->cap;
    if (pass()) return rc;
    if (n >= t->cap) return DK_TOOBIG;
    make_fn("", name, ",S,R");
    p = t->buf;
    pass();
    n = (uint16_t)(p - t->buf); /* what was stored, even after an error */
    t->len = n;                 /* text at the start, the gap after it */
    t->gs = n;
    t->cur = 0;
    t->want = 0;
    return rc;
}

uint8_t disk_save(const char *name, TextBuf *t, uint8_t replace)
{
    p = (char *)tb_text(t);
    begin();
    if (replace) {              /* scratch: S0:NAME on the command channel */
        make_fn("S0:", name, "");
        if (cbm_k_ckout(LF_CMD)) {
            close_all();
            return DK_NODEV;
        }
        for (i = 0; fn[i]; ++i) cbm_k_bsout(fn[i]);
        cbm_k_bsout(13);
        cbm_k_clrch();
        rc = status();
        if (rc) {
            close_all();
            return rc;
        }
    }
    make_fn("", name, ",S,W");
    rc = open_data(2);
    if (rc == 0 && cbm_k_ckout(LF_DATA)) rc = DK_NODEV;
    if (rc) {
        close_all();
        return rc;
    }
    while (*p) cbm_k_bsout(*p++);
    cbm_k_clrch();
    return dk_close();
}

const char *disk_message(uint8_t code)
{
    const char *s = 0;
    if (code == DK_TOOBIG) return "FILE TOO BIG";
    /* no drive; 74 DRIVE NOT READY; a 1541 without a disk also answers
     * 21 READ ERROR (no sync mark found) */
    if (code == DK_NODEV || code == 74 || code == 21) s = "NO DISK";
    else if (code == 62) s = "FILE NOT FOUND";
    else if (code == 63) s = "FILE EXISTS";
    else if (code == 72) s = "DISK FULL";
    if (s) {
        strcpy(mbuf, "DISK ERROR: ");
        strcat(mbuf, s);
    } else {
        strcpy(mbuf, "DISK ERROR 00");
        mbuf[11] = (char)('0' + code / 10);
        mbuf[12] = (char)('0' + code % 10);
    }
    return mbuf;
}
