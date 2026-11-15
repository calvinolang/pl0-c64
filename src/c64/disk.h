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
 * the error channel 15) or one of the codes below. Not reentrant; every
 * call closes what it opened (logical files 2 and 15). */
#include <stdint.h>
#include "c64/textbuf.h"

#define DK_NODEV  254   /* no drive answers (KERNAL: device not present) */
#define DK_TOOBIG 253   /* file longer than the text buffer */
#define DK_EXISTS 63    /* DOS 63 FILE EXISTS (disk_save without replace) */

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

/* Byte stream for the picker (directory, INDEX): dk_open opens fname on
 * secondary address sa (0 for "$", the directory as a BASIC listing; 2
 * for "NAME,S,R"), dk_getc returns the next byte or -1 at the end or on an
 * error, dk_close closes and returns the drive status (0 when fine). */
uint8_t dk_open(const char *fname, uint8_t sa);
int dk_getc(void);
uint8_t dk_close(void);

/* Plain message for a result code: "DISK ERROR: NO DISK", "FILE TOO BIG",
 * "DISK ERROR 21", ... in a static buffer. */
const char *disk_message(uint8_t code);

#endif
