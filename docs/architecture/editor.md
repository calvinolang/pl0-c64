# The C64 editor (Task 14)

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
| F1 | compile (Task 15: and run); an error comes back with the cursor on its line |
| F2 (SHIFT+F1) | new program; asks `DISCARD PROGRAM? (Y/N)` if the text is modified and not empty (N or RUN/STOP keeps it) |
| F3 / F5 | open / save (Task 15; now `NOT YET`) |
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
