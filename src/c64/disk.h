#ifndef PL0_DISK_H
#define PL0_DISK_H
/* Disk access for the C64 program (Task 15): SEQ files on the current
 * drive through the KERNAL file calls (docs/architecture/editor.md).
 *
 * Program files are SEQ files holding the program text as raw ASCII bytes
 * with '\n' ($0A) line ends, named with 1..12 of A-Z 0-9 - _ . and no
 * extension (FACTORIAL). Those characters are the same bytes in ASCII and
 * PETSCII, so names go to the KERNAL unchanged.
 *
 * Results: 0 success, otherwise the drive's DOS error code (20..99, from
 * the error channel 15) or one of the codes below. Not reentrant. Logical
 * files 2 (data) and 15 (command channel): disk_load and disk_save close
 * both before they return; of the primitives below, dk_open leaves 2 and
 * 15 open on success (dk_close closes them), dk_cmd leaves 15 open (the
 * next operation closes it first), and every failing call closes both. */
#include <stdint.h>
#include "c64/textbuf.h"

#define DK_NODEV  254   /* no drive answers (KERNAL: device not present) */
#define DK_TOOBIG 253   /* file longer than the text buffer */
#define DK_READ   252   /* the KERNAL reported a read error (READST) while
                           the drive status was fine: e.g. a serial timeout */
#define DK_EXISTS 63    /* DOS 63 FILE EXISTS (disk_save without replace) */

/* nonzero if s is a valid program file name: 1..12 of A-Z 0-9 - _ . */
uint8_t __fastcall__ disk_name_ok(const char *s);

/* set by disk_load once its second pass has started: from then on the
 * text buffer no longer holds the old text, even if the load fails */
extern uint8_t disk_touched;

extern uint8_t disk_dev;    /* device number: $BA at start-up, else 8 */
void disk_init(void);

/* Load file name into t (cursor at 0). Two passes: the first only counts
 * the bytes, so a file that is too big (DK_TOOBIG) or missing leaves t
 * unchanged. CR, LF and CR LF become one '\n', a tab a space, other bytes
 * outside $20-$7E are dropped. If the second pass fails, t holds what was
 * read so far. */
uint8_t disk_load(const char *name, TextBuf *t);

/* Save the text of t as name (tb_text: the gap moves, cursor stays). With
 * replace the old file is scratched first; without, an existing file
 * gives DK_EXISTS and nothing is written. */
uint8_t disk_save(const char *name, TextBuf *t, uint8_t replace);

/* KERNAL primitives (disk.s). dk_open opens fname as file 2: "$" (the
 * directory, as a BASIC listing), NAME,S,R for input, NAME,S,W for
 * output; 0 or a result code (then everything is closed again). dk_getc
 * returns the next byte or -1 at the end or on an error; dk_text reads
 * the rest as text into dst (0: only count), at most dk_lim bytes, and
 * returns the count; dk_write sends a NUL-terminated string; dk_close
 * closes and returns the drive status (0 when fine), or DK_READ when the
 * status was fine but dk_getc had stopped on a KERNAL read error. dk_cmd sends a DOS
 * command (S0:NAME) and returns the status. */
uint8_t __fastcall__ dk_open(const char *fname);
int dk_getc(void);
extern uint16_t dk_lim;
uint16_t __fastcall__ dk_text(char *dst);
void __fastcall__ dk_write(const char *s);
uint8_t dk_close(void);
uint8_t __fastcall__ dk_cmd(const char *s);

/* Plain message for a result code: "DISK ERROR: NO DISK", "FILE TOO BIG",
 * "DISK ERROR 21", ... in a static buffer. */
const char *disk_message(uint8_t code);

#endif
