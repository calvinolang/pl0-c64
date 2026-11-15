#ifndef PL0_EDITOR_H
#define PL0_EDITOR_H
/* The C64 editor screen (Task 14; key bindings: docs/architecture/editor.md).
 *
 * Row 0 status bar (program name, `*` when modified, line and column),
 * rows 1-23 the text (one buffer line per row, the cursor's row scrolls
 * horizontally), row 24 the key bar or a message. The buffer is ASCII;
 * PETSCII exists only at the keyboard, screen codes only on the screen. */
#include <stdint.h>
#include "c64/textbuf.h"
#include "core/errors.h"

typedef enum { ED_RUN, ED_OPEN, ED_SAVE } EdAction;

/* Edit t until the user presses F1 (ED_RUN), F3 (ED_OPEN) or F5 (ED_SAVE).
 * F7 (help) and F2 (new program) are handled inside. F2 and F3 ask
 * `DISCARD PROGRAM? (Y/N)` first when the text is modified and not empty;
 * F3 returns only after Y. With err_or_null the
 * cursor goes to the start of that line and the message
 * `ERROR line N: text` shows on row 24 until the next key. The text may
 * have been replaced between calls (tb_set): the view is rebuilt from the
 * cursor on every call. */
EdAction editor_run(TextBuf *t, const PL0Error *err_or_null);

#define EDITOR_NAME_MAX 16          /* C64 file names; the bar shows 12 */
/* NULL or "" means unnamed (the bar shows NEW). Longer names are cut. */
void editor_set_name(const char *name);
const char *editor_name(void);      /* "" when unnamed */
/* the modified flag (set by any edit; the editor reads it itself for
 * F2 and F3, so there is no getter: it would cost bytes unused) */
void editor_set_modified(uint8_t m);  /* 0 after a save or a load */
#define editor_clear_modified() editor_set_modified(0)
/* One-off message for row 24 (at most 40 characters shown), shown by the
 * next editor_run until the next key; s must stay valid until then. */
void editor_set_message(const char *s);
void editor_set_alert(const char *s);   /* the same in the error colour */

#endif
