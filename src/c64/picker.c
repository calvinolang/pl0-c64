/* picker.c -- the F3 list of programs (picker.h).
 *
 * Screen (40x25): row 0 the title bar ` OPEN PROGRAM` (reverse), rows
 * 2-22 the entries (21 visible), each ` NAME` (12 columns) and the
 * description from column 14, the selected one in reverse video; row 24
 * the key bar. CRSR down/up move the selection and stop at the ends (no
 * wrap: holding a key never jumps from the last entry to the first); the
 * list scrolls one entry at a time to keep the selection visible.
 *
 * The list (read by picker.s, which describes the formats): INDEX first (`NAME description` lines, the examples in demo
 * order), then every other SEQ file of the directory with a valid name
 * (disk.h; not INDEX, not an example again), sorted by name, without a
 * description (reading each file's first comment would cost a file open
 * per entry: seconds on a 1541). Other file types (the PL0 program) are
 * never listed; so are names the program could not save (lower case,
 * spaces, more than 12 characters). The entries live in memory the
 * caller lends (main.c: the Program, idle while no program is compiled
 * or run); extra files beyond max are left out.
 *
 * Directory parsing: "$" on secondary address 0 is a BASIC listing:
 * load address, then lines of link (2 bytes, 0 0 ends), block count (2)
 * and text up to a 0 byte: `"NAME"   SEQ` for a file, the disk title in
 * quotes on the first line (skipped) and `BLOCKS FREE.` (no quotes) last.
 * The type is the first 3 characters after the closing quote that are not
 * spaces; an unclosed file shows `*SEQ` and is skipped.
 *
 * An entry is kept as the screen row it shows (PICK_ROW = 40 bytes, no
 * NUL: ed_put stops after 40 characters): ` NAME` padded to column 13,
 * the description from column 14; the name is read back up to its first
 * space.
 *
 * Drawing through edscreen.s (ed_put: ASCII to screen codes). ASCII
 * charmap like every C64 source; PETSCII key codes are numbers. */
#include <string.h>
#include <cbm.h>
#include "core/ascii.h"
#include "c64/picker.h"
#include "c64/disk.h"

#define REG(a)  (*(volatile uint8_t *)(a))
#define SCREEN  ((uint8_t *)0x0400)
#define COLRAM  ((uint8_t *)0xD800)
#define NDX     REG(0xC6)
#define RPTFLG  REG(0x028A)
#define C_TEXT  14
#define FIRST   2               /* first list row */
#define ROWS    21              /* rows 2..22 */
#define DESC    14              /* column of the description */

extern uint8_t *ed_scr;
extern uint8_t ed_rev;
void __fastcall__ ed_put(const char *s);
/* picker.s */
extern char *pk_list;
extern uint8_t pk_max, pk_n;
uint8_t pk_read(void);
char *__fastcall__ pk_at(uint8_t i);

#pragma bss-name (push, "LOWBSS")      /* the cassette buffer: main.c */
static uint8_t sel, top, r, i, old_rpt, key;
static char *e;
#pragma bss-name (pop)

void ui_bar(const char *s, uint8_t colour)
{
    ed_scr = SCREEN + 960;
    ed_rev = 0x80;
    ed_put(s);
    ed_rev = 0;
    memset(COLRAM + 960, colour, 40);
}

static void draw(void)
{
    for (r = 0; r < ROWS; ++r) {
        i = (uint8_t)(top + r);
        ed_scr = SCREEN + (uint16_t)(r + FIRST) * 40;
        ed_rev = 0;
        if (i == sel) ed_rev = 0x80;
        if (i < pk_n) ed_put(pk_at(i));
        else ed_put("");
    }
    ed_rev = 0;
}

uint8_t picker_run(char *list, uint8_t max, char *name)
{
    pk_list = list;
    pk_max = max;
    ui_bar("READING DISK...", 1);
    key = pk_read();
    if (key) return key;
    if (pk_n == 0) return PK_EMPTY;

    memset(COLRAM, C_TEXT, 1000);
    ed_scr = SCREEN;
    ed_rev = 0x80;
    ed_put(" OPEN PROGRAM");
    ed_rev = 0;
    ed_scr = SCREEN + 40;
    ed_put("");
    ed_scr = SCREEN + 920;
    ed_put("");
    ui_bar(" RETURN open  RUN/STOP back  CRSR move", C_TEXT);
    sel = top = 0;
    old_rpt = RPTFLG;
    RPTFLG = 0x80;              /* held CRSR keys repeat */
    NDX = 0;
    for (;;) {
        if (NDX == 0) draw();
        do key = cbm_k_getin(); while (key == 0);
        if (key == 0x11) {                          /* CRSR down */
            if (sel + 1 < pk_n) ++sel;
            if (sel >= (uint8_t)(top + ROWS)) ++top;
        } else if (key == 0x91) {                   /* CRSR up */
            if (sel) --sel;
            if (sel < top) top = sel;
        } else if (key == 0x0D || key == 0x03) {    /* RETURN, RUN/STOP */
            break;
        }
    }
    RPTFLG = old_rpt;
    if (key == 0x03) return PK_BACK;
    e = pk_at(sel) + 1;                             /* the name: to a space */
    for (i = 0; e[i] != ' '; ++i) name[i] = e[i];
    name[i] = 0;
    return 0;
}
