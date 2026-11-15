# The C64 editor (Task 14), run flow, picker and disk (Task 15)

`src/c64/editor.c` (logic, keys), `src/c64/edscreen.s` (gap buffer scans
and drawing), `src/c64/main.c` (start-up and the F1/F3/F5 hooks).
Screenshots: `docs/img/editor.png`, `docs/img/editor-error.png`.

## Screen

```
 PL/0   SQUARE*         L 13  C 1        row 0: status bar (reverse)
{ a square }                             rows 1-23: the text
...
ERROR line 13: undeclared name           row 24: key bar or message
```

- Status bar: program name (12 characters shown, `NEW` when unnamed), `*`
  when modified, line and column (both from 1; `L` always in column 24).
- Key bar: ` F1 Run F2 New F3 Open F5 Save F7 Help`. A message replaces it
  until the next key, which then also does its usual job: errors and
  `BUFFER FULL` in light red, the F2 question in yellow, other messages in
  white.
- One buffer line per row. Only the cursor's row scrolls sideways: `<` in
  column 0 when characters are hidden on the left; any row whose line goes
  on past the window shows `>` in column 39. The cursor stays in columns
  1-38 of a scrolled row, never under a marker.
- The view scrolls one line at a time to keep the cursor on rows 1-23;
  after an error the line is shown with up to 7 lines above it.
- The cursor is a steady reverse-video cell.
- Colours: the C64 default, light blue on blue.
- Character set: upper/lower case. `{` and `}` show as `[` and `]` (the C64
  has no braces).

## Keys

| Key | Action |
|---|---|
| letters | lower case; with SHIFT upper case (PL/0 ignores case) |
| digits, punctuation | as typed; SHIFT+SPACE types a space |
| SHIFT+: and SHIFT+; | `{` and `}` (comments), shown as `[` `]` |
| RETURN (or SHIFT+RETURN) | new line |
| CRSR keys | move; up/down keep the column of the first vertical move |
| HOME / CLR (SHIFT+HOME) | start / end of the line |
| DEL / INST (SHIFT+DEL) | delete left / delete right (join lines at the ends) |
| F1 | compile and run (below); an error comes back with the cursor on its line |
| F2 (SHIFT+F1) | new program; asks `DISCARD PROGRAM? (Y/N)` if the text is modified and not empty (N or RUN/STOP keeps it) |
| F3 | open a program from the disk (the picker, below); asks `DISCARD PROGRAM? (Y/N)` first like F2 |
| F5 | save to the disk (below) |
| F7 | help page; any key returns |
| anything else | ignored (colours, graphics, RVS, RUN/STOP, F4/F6/F8) |

All keys repeat while held (KERNAL `$028A = $80`), and SHIFT+C= cannot
switch the character set (`$0291 = $80`); both are restored when the
editor returns. Keys typed before the editor shows (during a compile or a
run) are dropped.

## How it works

- The editor works on a copy of the caller's `TextBuf`, `ed_tb`, and copies
  it back when it returns: cc65 reaches the fields of a static object with
  absolute addressing, and `edscreen.s` reads them at fixed offsets
  (checked at compile time).
- It caches the cursor's line number and line start and the view's top
  line and offset, and updates them from the key (one line scanned at
  most). `editor_run` rebuilds them from the cursor on entry (the text may
  have been replaced): one backward pass over the text before the cursor.
- The textbuf's own line scans read through `tb_at`, about 1000 cycles per
  character in cc65 code, so the editor does vertical moves, HOME and CLR
  with `ed_bol`/`ed_eol` (assembly, about 40 cycles per character) and keeps
  the sticky column itself, with the same rule as `tb_up`/`tb_down`.
- Keys waiting in the keyboard buffer are all handled before the next
  redraw. A redraw covers every row they changed: one edited row
  (`D_ROW`), or everything from the first changed line down (`D_ALL` from
  `fline`: a new, split or joined line, or a second edited row), or all
  23 rows after a scroll.
- Measured redraw times (PAL cycles, CIA2 timers, KERNAL interrupts
  included):

  | Redraw | Text | Rows | Whole frame |
  |---|---|---:|---:|
  | 23 rows | starter (short lines) | 29,000 | 41,000 |
  | 23 rows | 23 lines of 56 characters (worst case) | 48,000 | 59,000 |
  | from the cursor down | RETURN at the end of the starter | 4,600 | 14,000 |
  | one row | a typed character | 1,500-2,500 | 7,500-11,000 |

  Most of each frame beyond the rows is the status bar and the KERNAL
  interrupt. Entering the editor (placing the cursor and view) costs about
  40 cycles per character before the cursor: 0.3 s for 8 KB.
- Memory: the editor's variables are in `HIBSS` (`$C000-$CBFF`), which
  `main` clears (crt0 does not); see `memory-map.md`.

## Run flow (F1, Task 15, `src/c64/main.c`)

- F1 compiles the text (`pl0_compile` on `tb_text`). A compile error goes
  back to the editor at once: message on row 24, cursor on its line.
- Otherwise the text screen is cleared and the program runs; its output
  scrolls. The first turtle statement switches to the bitmap and the run
  goes on there (output still goes to the hidden text screen).
- RUN/STOP stops a running program: the runtime error `stopped`.
- At the end:
  - runtime error (also `stopped`): the bitmap is switched off, the text
    screen shows the output, then `ERROR line N: message` and
    `-- PRESS ANY KEY --` on new lines; any key returns to the editor with
    the error on row 24 and the cursor on that line;
  - normal end without a drawing: `-- PRESS ANY KEY --` on a new line;
    any key returns;
  - normal end with a drawing: the drawing stays (no overlay); SPACE
    toggles between the drawing and the text view, which ends with
    `-- SPACE: DRAWING  ANY KEY: EDITOR --`; any other key returns. The
    toggle is offered after the run has ended (not while it runs).
  Keys typed during the run are dropped before the prompt. The help page
  (F7) says this too.
- Returning: `gfx_close`, then the editor redraws everything. A run never
  changes the text or the editor state (checked in `tests/vice_app.sh`:
  the buffer equals the loaded example byte for byte after a run).
- A program that reads input (`?x`) after it has drawn reads it on the
  hidden text screen; no example does that.

## Disk (Task 15, `src/c64/disk.{c,s}`)

- Device: the one the program was loaded from (`$BA`), else 8.
- Program files are SEQ files with the program text as raw ASCII bytes,
  `\n` line ends, so a PC reads them with `c1541 IMAGE -read name,s`.
  Names: 1-12 of `A-Z 0-9 - _ .`, no extension on the disk (`FACTORIAL`,
  not `FACTORIAL.PL0`). These are the same bytes in ASCII and PETSCII.
- KERNAL calls only (SETLFS, SETNAM, OPEN, CHKIN, CHKOUT, CHRIN, CHROUT,
  READST, CLRCHN, CLOSE), in assembly. Channel 15 gives the drive status
  after every open, command and close. A missing drive (KERNAL error 5),
  DOS 74 DRIVE NOT READY and DOS 21 READ ERROR (what a 1541 without a
  disk answers) all show as `NO DISK`.
- Load: two passes. The first only counts (and stops once the file is too
  big: `FILE TOO BIG`, nothing changes); the second stores into the buffer
  under the BASIC ROM. CR, LF and CR LF become one `\n`, a tab a space,
  other bytes outside `$20-$7E` are dropped. If the second pass fails, the
  buffer holds the part that was read, named NEW and marked modified.
  Cursor on line 1, column 1, not modified, name set.
- Save: opening `NAME,S,W` answers 63 FILE EXISTS before anything is
  written; the program then asks. A replace scratches the file (`S0:NAME`)
  and writes it again (the 1541's `@0:` save-with-replace has a known
  bug). After the close the status is read: `DISK ERROR: FILE NOT FOUND`
  (62), `FILE EXISTS` (63), `DISK FULL` (72), `NO DISK`, other codes
  `DISK ERROR nn`.

## Save prompt (F5)

Row 24 shows `SAVE AS: NAME` with the current name and a plain (not
reversed) cell as the cursor. Letters (typed upper case either way),
digits and punctuation are taken, up to 16 characters; DEL deletes;
RETURN saves; RUN/STOP cancels. The name is checked then: empty, longer
than 12 or any character outside `A-Z 0-9 - _ .` gives `BAD NAME` and
writes nothing. An existing file asks `REPLACE? (Y/N)` (N or RUN/STOP:
`NOT SAVED`). Success: `SAVED NAME`, the program takes the name, not
modified.

## Picker (F3, `src/c64/picker.{c,s}`)

```
 OPEN PROGRAM                            row 0 (reverse)
                                         row 1
 SQUARE       a square                   rows 2-22: 21 entries
 POLYGON      regular polygon            (the selected one reversed)
 ...
 BIG
 TEST
                                         row 23
 RETURN open  RUN/STOP back  CRSR move   row 24 (reverse)
```

- Order: the examples from `INDEX`, in its order (the demo order of
  `tools/mkdisk.py`), then every other SEQ file with a valid name, sorted
  by name, without a description (reading each file's first comment
  would take a file open per entry: seconds on a 1541). `INDEX`, the
  `PL0` program and names the program could not save are not listed; a
  file of the user's own named like an example is listed once, in the
  example's place. A disk without INDEX lists only the SEQ files.
- CRSR down/up move and stop at the ends (no wrap); the list scrolls one
  entry at a time. Held keys repeat. RETURN loads the entry; RUN/STOP goes
  back with nothing changed.
- `READING DISK...` and `LOADING NAME` show on row 24 while the drive
  works. No disk: back in the editor with `NO DISK`; no programs:
  `NO PROGRAMS`.
- Memory: the list (at most 118 entries of 40 bytes, kept as the screen
  rows they show) lives in the `Program` memory, idle outside a compile
  and run (`main.c`: a union; only `open_program` uses the list and only
  `run_program` the program, both called one at a time from the main
  loop). A 1541 directory holds at most 144 files; entries past 118 are
  left out.
