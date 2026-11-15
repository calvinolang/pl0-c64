#ifndef PL0_PICKER_H
#define PL0_PICKER_H
/* The F3 picker (Task 15): a list of the programs on the disk, the
 * examples first (from the INDEX file, in its order), then the user's
 * own SEQ files (alphabetical). docs/architecture/editor.md. */
#include <stdint.h>

/* An entry is the 40-character screen row it shows (picker.c). */
#define PICK_ROW 40

#define PK_BACK  1      /* RUN/STOP: nothing chosen */
#define PK_EMPTY 2      /* the disk holds no programs */

/* Read INDEX and the directory into list (at most max entries of
 * PICK_ROW bytes; the
 * caller lends memory: main.c's idle Program), show the list and let the
 * user choose. Returns 0 with the chosen name in name (13 bytes), PK_BACK,
 * PK_EMPTY, or a disk result code (disk.h) when the disk cannot be read. */
uint8_t picker_run(char *list, uint8_t max, char *name);

/* Row 24 of the screen: s (at most 40 characters) in reverse video and
 * the given colour; used for the prompts of F3 and F5. */
void ui_bar(const char *s, uint8_t colour);

#endif
