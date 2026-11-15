# Spike results: C64 memory budget and BASIC ROM floats from cc65

Task 1 of `docs/superpowers/plans/2026-11-15-pl0-c64.md`. The spike code in
`spikes/rom-float/` is throwaway; only the knowledge below is reused. Every
number here comes from a linker map or a headless VICE run made by
`spikes/rom-float/run.sh`, unless the text marks it as an **estimate**.

Tools: cc65 2.18 (Ubuntu package 2.19-1), VICE 3.7.1 `x64sc`, PAL C64, stock
ROMs (BASIC 901226-01, KERNAL 901227-03).

## 0. Decisions at a glance

| Question | Answer |
|---|---|
| Does cc65's default C64 layout keep the BASIC ROM visible? | **No.** crt0 switches BASIC out (`$01` = `$36` when `main` starts, measured) and the stock `c64.cfg` puts RAM up to `$CFFF` (`__HIMEM__ = $D000`) to use. |
| How do we call the ROM then? | The shims bank BASIC in only while a ROM routine runs (`$01 \|= 3`, restored afterwards). Works with any link layout. |
| Link config | `spikes/rom-float/pl0-c64.cfg` (stock config with `__HIMEM__ = $A000`, plus BSS-only areas `BASICBSS` `$A000-$BFFF` and `HIBSS` `$C000-$CBFF`). Invocation: `cl65 -t c64 -O -C pl0-c64.cfg ...` |
| Program must avoid `$A000-$BFFF`? | The program image, ordinary BSS, the C stack and every float the ROM touches: **yes**. The RAM under the BASIC ROM is still usable for data the ROM never sees (proposal: the source text), see section 3. |
| Bitmap location | VIC bank 3: bitmap `$E000-$FF3F` (RAM under the KERNAL), colour matrix `$CC00-$CFE7`. Costs no byte of `$0801-$9FFF`, and the text screen at `$0400` survives a drawing. Verified in VICE. |
| Draft limits (2048 code, 64 idents, 1024 stack, 16 KB source) | **Do not fit.** Even the weight stub alone (no compiler yet) overflows the `$A000` limit by 7362 bytes. |
| Proposed limits | `MAX_CODE 768`, `MAX_IDENTS 64`, `MAX_STACK 512`, `MAX_NEST 4`, `MAX_SOURCE 8192`, `MAX_CONSTS 128`, `MAX_STRINGS 1024` (section 7). |
| VM in C or assembly? | **GO: keep the VM in C**, written to the cc65 rules in section 8, with a 5-byte copy primitive in assembly. Measured: 505 µs per LOD/STO in tuned C against 210 µs in an assembly loop and 1077 µs in naive C. The dispatch loop moves to assembly only if Task 15 timings miss the 30 s limit. |
| Embed the 12 examples (about 7.2 KB)? | **No.** Put them on the `.d64` as SEQ files and load them from the picker. 7.2 KB does not fit next to the full program with a safe margin. |
| P-code step budget for Task 10 | The provisional 2,000,000 instructions is far too high. At 0.5–1.3 ms per instruction, 30 s is roughly **25,000–50,000 P-code instructions**. ROM `SIN` costs 28 ms. |

## 1. How to reproduce

```sh
cd spikes/rom-float
./run.sh          # builds everything (make), runs four kinds of programs in VICE
```

`run.sh` runs `x64sc -console -default -warp -sounddev dummy -limitcycles N
-remotemonitor -moncommands <file> -autostartprgmode 1 -autostart <prg>
-exitscreenshot <png>`, with no X display needed for output. `vicemon.py`
loads the ld65 label file (`-Ln`), sets a breakpoint (normally `._spike_done`),
and, when VICE stops there, sends monitor commands over the text remote
monitor (`bsave`, `m`, `d`, `screenshot`) and then `quit`. The programs write
their results into a `logbuf[]` array that is saved with `bsave` and decoded
by `petscii.py`. A full run takes about 50 s of wall time. Outputs go to
`spikes/rom-float/out/`:

| File | Content |
|---|---|
| `spike.txt` | ROM float results, error traps, ZP clobber list, timings |
| `rom-disasm.txt` | first instructions of every ROM entry used, from the monitor with `bank rom` |
| `weight*.txt` | VM benchmarks and the RAM-under-BASIC check, one per VM variant |
| `gfx-bitmap.png`, `gfx-text.png`, `gfx-regs.txt`, `gfx-textscreen.txt` | bitmap test |
| `sizes.txt`, `*.map` | segment lists of every build; `weight-draft-pl0.txt` holds the ld65 overflow error |

Things that did not work, so that later tasks need not rediscover them:
- Feeding monitor commands on stdin with `-console`: VICE does not read them.
  Use `-remotemonitor`.
- `break .shown_text`: C symbols carry an underscore (`._shown_text`).
  Monitor address expressions such as `._logbuf+100` are rejected, so
  `run.sh` computes the addresses itself.
- A monitor `screenshot` taken right after the program changes video
  registers shows the old frame. Wait for 2–3 frames (jiffy clock `$A2`)
  before the breakpoint.
- **cc65 `conio` does not scroll.** Printing more than 25 lines with
  `cputs`/`cprintf` writes past `$07E7` into the program at `$0801`; this
  crashed the first spike run. Use `stdio` (KERNAL CHROUT scrolls) or manage
  the screen yourself. The editor (Task 14) must clamp rows.

## 2. Linker config and memory layout

### Stock cc65 C64 layout (measured, `/usr/share/cc65/cfg/c64.cfg`)

- `$0801` BASIC stub (`EXEHDR`, 12 bytes, `SYS 2061`), then `STARTUP`
  (`$080D`), `LOWCODE`, `CODE`, `RODATA`, `DATA`, `INIT`, `ONCE`.
- `BSS` starts at `__ONCE_RUN__` (it overlays the run-once init code).
- Heap: from the end of BSS to the bottom of the C stack. C stack: 2 KB
  (`__STACKSIZE__ = $0800`) growing down from `__HIMEM__` (stock `$D000`, so
  `$C800-$CFFF`).
- crt0 switches BASIC out: `$01` is `$36` at `main` (printed by `spike.prg`:
  `$01=36`). The value is restored on exit.
- So with the stock config, code and BSS freely extend into `$A000-$BFFF`
  (the draft-limit stub's BSS ends at `$B4C1`, `weight-draft-stock.map`), and
  any float placed there would be read as ROM by the ROM routines.

### Proposed config: `spikes/rom-float/pl0-c64.cfg`

Copy this file to the real build (Task 11/16). It is the stock config with:
- `__HIMEM__ = $A000`. Code, data, BSS, heap and the 2 KB C stack all live
  in `$0801-$9FFF`. The C stack is `$9800-$9FFF`.
- `BASICRAM` `$A000-$BFFF` (segment `BASICBSS`) and `HIRAM` `$C000-$CBFF`
  (segment `HIBSS`): BSS-only, `define = yes`, `optional = yes`. Place
  arrays there with:

  ```c
  #pragma bss-name (push, "HIBSS")
  Instr code[MAX_CODE];
  #pragma bss-name (pop)
  ```

  **crt0 does not zero these segments**; clear them at startup if needed.
- `$CC00-$CFFF` stays outside every memory area: it is the bitmap colour
  matrix.

Invocation: `cl65 -t c64 -O -C pl0-c64.cfg -m pl0.map -Ln pl0.lbl -o pl0.prg <sources>`.
Keep `-m` in the real build: the map is the memory gate (BSS must end at or
below `$97FF`, `HIBSS` within `$CBFF`).

### Free RAM (from the maps)

| Area | Range | Bytes | Use |
|---|---|---|---|
| Program image + BSS + heap | `$080D-$97FF` | 36,851 | code, rodata, data, ordinary BSS |
| C stack | `$9800-$9FFF` | 2,048 | cc65 parameter stack (default size) |
| `BASICBSS` (RAM under BASIC ROM) | `$A000-$BFFF` | 8,192 | proposed: source text buffer |
| `HIBSS` | `$C000-$CBFF` | 3,072 | proposed: P-code array |
| Bitmap colour matrix | `$CC00-$CFE7` | 1,000 | VIC bank 3 |
| I/O | `$D000-$DFFF` | | |
| Bitmap (RAM under KERNAL) | `$E000-$FF3F` | 8,000 | VIC bank 3 |

The "about 38 KB" in the spec is the 38,899 bytes from `$080D` to `$9FFF`
(BASIC reports 38,911 bytes free from `$0801`).

## 3. RAM under the BASIC ROM (proposal that relaxes a Global Constraint)

The plan's Global Constraints say "nothing may be placed at
`$A000–$BFFF`". Because the shims bank BASIC in **only during a ROM call**,
C code normally runs with `$01 = $36` and sees RAM at `$A000-$BFFF`. Spec
10b(1) lists "bank RAM under the ROMs" as an allowed response when memory is
short, and it is short (section 6). Measured in `weight*.prg`: 8192 bytes of
pattern written to `source[]` at `$A000-$BFFF`, then 50 ROM divisions, then the
whole pattern read back: `bad=0`, result `1/3=.333333333`, `$01=36`.

Rules if this is adopted (the controller or author should confirm, since it
changes a Global Constraint):
1. Only data the ROM never touches goes there: the editor's text buffer is
   the natural candidate. Never pass a pointer into `BASICBSS` to an `fp_*`
   shim. In particular `num_parse` must copy the literal into a buffer in low
   RAM before calling `FIN` (the lexer copies token text anyway).
2. No interrupt handler may switch BASIC in (the KERNAL IRQ does not).
3. KERNAL `SAVE` reads through `($xx),Y` with the current `$01`, so it saves
   RAM from `$A000` directly; `LOAD` always writes RAM. Task 15 should check
   this with a save/load round trip in VICE (not done in the spike).

If it is not adopted, the source buffer has to go into low RAM and the margin
in section 6 disappears; then `MAX_SOURCE` must drop to about 4096 and the
other limits shrink, or the VM must move to assembly to free code space.

## 4. Calling the BASIC ROM floating point (proven)

### The pattern (`spikes/rom-float/fp.s`)

Each C-callable shim (`__fastcall__`, pointer in A/X) loads A/Y or X/Y as
the ROM wants, patches the operand of one `JSR` and jumps to `callrom`:

1. `TSX` / save SP; save `$0300/$0301` (IERROR) and point it at `trap`.
2. Save `$01`, then `ORA #$03` so BASIC + KERNAL are visible.
3. `JSR` the ROM routine with A/X/Y passed through; store its A/X/Y.
4. Restore `$01` and `$0300`; return A = 0.
5. On a BASIC error the ROM does `LDX #err : JMP $A437`, and `$A437` is
   `JMP ($0300)` (verified), so `trap` runs with X = error number. It
   reloads the saved SP (`TXS`) and leaves through step 4, returning
   A = the error number. Afterwards everything works normally (measured:
   `after errors 1/3 = .333333333`, `$0300` back to `$E38B`).

FAC (`$61-$66`) survives between shims: cc65 code never touches it, so
`fp_load(a); fp_add(b); fp_store(r)` is a valid sequence. Task 11 can still
keep each `num_*` call self-contained (load, op, store).

### ROM entry points (addresses checked in VICE with `bank rom`, `out/rom-disasm.txt`)

| Name | Addr | In | Out / effect | First bytes seen |
|---|---|---|---|---|
| MOVFM | `$BBA2` | A/Y = ptr | FAC = mem | `STA $22; STY $23; LDY #4` |
| CONUPK | `$BA8C` | A/Y = ptr | ARG = mem (called by FADD etc.) | same shape |
| MOVMF | `$BBD4` | X/Y = ptr | mem = FAC, **rounded** first (`JSR $BC1B`) | `JSR $BC1B; STX $22` |
| FSUB | `$B850` | A/Y = ptr | FAC = mem − FAC | `JSR $BA8C` |
| FADD | `$B867` | A/Y = ptr | FAC = mem + FAC | `JSR $BA8C; BNE; JMP $BBFC` |
| FMULT | `$BA28` | A/Y = ptr | FAC = mem × FAC | `JSR $BA8C` |
| FDIV | `$BB0F` | A/Y = ptr | FAC = mem / FAC; FAC=0 → error 20 | `JSR $BA8C; BEQ $BB8A` |
| FOUT | `$BDDD` | FAC | ASCII at `$0100`, NUL-terminated; returns A=`$00`, Y=`$01` | `LDY #1; LDA #$20` |
| GIVAYF | `$B391` | A = hi, Y = lo | FAC = signed 16-bit int | `LDX #0; STX $0D` |
| FACINX | `$B1AA` | FAC | A = hi, Y = lo; outside −32768..32767 → error 14 | `JSR $B1BF` |
| FIN | `$BCF3` | via CHRGET | FAC = number at TXTPTR (`$7A/$7B`) | `LDY #0; LDX #$0A` |
| FCOMP | `$BC5B` | A/Y = ptr | A = 0 equal, 1 FAC > mem, `$FF` FAC < mem | `STA $24; STY $25` |
| ABS | `$BC58` | FAC | `LSR $66` (clear sign) | |
| INT | `$BCCC` | FAC | floor | |
| SQR | `$BF71` | FAC | sqrt (computed as x^0.5 via LOG/EXP) | |
| EXP | `$BFED` | FAC | e^x | |
| LOG | `$B9EA` | FAC | ln x; x ≤ 0 → error 14 | `JSR $BC2B; BEQ/BPL; JMP $B248` |
| NEGOP | `$BFB4` | FAC | −FAC (0 stays 0) | |
| COS | `$E264` | FAC | adds π/2, falls into SIN | |
| SIN | `$E26B` | FAC | sin (radians) | |
| ATN | `$E30E` | FAC | arctan | |
| ERROR | `$A437` | X = err | `JMP ($0300)` | `6C 00 03` |

`FIN` needs `TXTPTR` (`$7A/$7B`) pointed at the string and `JSR CHRGOT`
(`$0079`) first. `TXTPTR` is where BASIC resumes after our `SYS`, so the shim
saves and restores it. After `FIN`, `TXTPTR` points past the number (measured:
`"12.5xyz"` → 4 characters used, value 12.5).

### Results (from `out/spike.txt`; packed bytes are exponent + 4 mantissa bytes)

| Expression | FOUT | Packed bytes |
|---|---|---|
| 1/3 | ` .333333333` | `7f 2a aa aa ab` |
| sqr(2) | ` 1.41421356` | `81 35 04 f3 34` |
| sin(1) | ` .841470985` | `80 57 6a a4 78` |
| cos(0) | ` 1` | `80 7f ff ff ff` (= 0.99999999977, **not exactly 1**) |
| 4·atn(1) | ` 3.14159266` | `82 49 0f da a2` |
| exp(1) | ` 2.71828183` | |
| log(10) | ` 2.30258509` | |
| int(−2.5) | `-3` | floor |
| abs / neg(−2.5) | ` 2.5` | |
| 0.1+0.2 | ` .3` | `7f 19 99 99 9a` |
| 1e9 | ` 1E+09` | |
| 123456789 | ` 123456789` | |
| 0.001 | ` 1E-03` | |
| 0.01 | ` .01` | |
| −1.5e−7 | `-1.5E-07` | |
| FACINX(−1234.7) | −1235 | rounds toward −∞ |
| FCOMP 2:3, 3:3, 10:3 | −1, 0, 1 | |
| 1e−39 | ` 0` | `00 20 00 00 00`: underflow to 0, **mantissa bytes not cleared** |
| sin(1e10) | ` 0` | argument too large, all precision lost |

(The ROM writes `E` in ASCII; the log decoder shows it in lower case.)

### Errors (all trapped; nothing printed, BASIC never entered)

| Case | ROM error | Pre-check in C |
|---|---|---|
| x / 0 | 20 DIVISION BY ZERO | divisor `b[0] == 0` |
| sqr(−1) | 14 ILLEGAL QUANTITY | `b[0] != 0 && (b[1] & 0x80)` |
| log(0), log(−1) | 14 | `b[0] == 0 \|\| (b[1] & 0x80)` |
| exp(100) | 15 OVERFLOW | cannot be checked cheaply: rely on the trap |
| 1e38 × 10 | 15 | trap |
| parse "1e39" | 15 | trap |
| FACINX(40000) | 14 | range-check first or use the trap |

Recommendation for Task 11: do the cheap pre-checks (clear messages for
`E_DIV_ZERO` and `E_DOMAIN`) **and** keep the trap in the shim as the safety
net that maps error 15 to `E_OVERFLOW`. Without the trap an overflow would
print `?OVERFLOW ERROR` and warm-start BASIC, abandoning the program.

### Zero page

With instrumentation on (every exercised routine, error paths included),
the ROM wrote only `$07` (cc65 `regsave+1`) and `$0D` (`ptr2+1`) inside
cc65's zero page `$02-$1B`. Both are caller-scratch in cc65's convention.
`sp` (`$02-$03`) and `regbank` (`$16-$1B`) were never touched. So the real
shim needs no zero-page save and restore (the spike's `fp_instrument` copy
loop can go). The ROM also uses `$22-$2A`, `$57-$70`, `$71-$72`,
`$7A/$7B` and `$0100-$010F`; cc65 uses none of these.

### Format: ROM FOUT versus the host rules (`src/host/num_host.c`)

FOUT puts a space before positive numbers, drops the leading zero
(` .333333333`), and switches to exponent form below 0.01 (` 1E-03`). The host
prints `0.333333333`, `0.001`, and uses an exponent only below 1e−4 or from
1e9 up. Both give at most 9 significant digits. For byte-identical output,
Task 11 should take the digits and decimal exponent from FOUT's string and
render them with the host rules in portable C. The other way round, digit
extraction by repeated ROM multiply, would be slower.

### Timings (PAL, 1 jiffy = 1/60 s, 500 calls each, instrumentation off)

The harness loop alone (`nop` = C loop + `switch` + an empty `callrom`)
costs 266–300 µs. "Net" subtracts that.

| Operation | Gross µs | Net µs (≈ cycles) |
|---|---|---|
| load (MOVFM) | 366 | ~100 |
| load + add + store | 1200 | ~930 |
| load + mul + store | 2166 | ~1900 |
| load + div + store | 3400 | ~3130 |
| load + sin + store | 27,800 | ~27,500 |
| load + sqr + store | 53,500 | ~53,200 |
| load + FOUT | 8,933 | ~8,650 |
| load + FCOMP | 700 | ~430 |
| int → float + store | 900 | ~630 |
| FIN parse + store | 22,700 | ~22,400 |

Consequences:
- **Turtle (Task 9/12):** `forward` needs sin and cos (about 56 ms). Compute
  them lazily, only when the heading changed since the last `forward`, and
  never in `turn`.
- **Pixel coordinates:** results such as cos(0) = 0.99999999977 mean
  `num_to_i16` (floor) gives 49 for "50". Round with +0.5 before floor when
  converting turtle coordinates to pixels.
- **Comparisons:** compare in C on the packed bytes (`numw.c` does). Zero is
  `b[0] == 0` whatever the other bytes hold (see 1e−39 above). This needs no
  banking or trap setup. It was not timed separately; FCOMP costs about 430 µs.
- `FIN` costs 22 ms per literal, which is acceptable at compile time
  (100 literals ≈ 2 s).

## 5. Bitmap and screen memory map (verified)

### Why VIC bank 3

The VIC sees 16 KB at a time (`$DD00` bits 0–1). Character ROM appears to
the VIC at `$1000-$1FFF` in bank 0 and `$9000-$9FFF` in bank 2.

| Bank | Range | Problem |
|---|---|---|
| 0 | `$0000-$3FFF` | Bitmap must sit at `$2000`, in the middle of the program (it starts at `$0801`). |
| 1 | `$4000-$7FFF` | Splits the program area the same way. |
| 2 | `$8000-$BFFF` | A bitmap at `$8000` overlaps the char-ROM shadow at `$9000` (the VIC would show ROM). `$A000` is the BASIC area. |
| **3** | `$C000-$FFFF` | Bitmap at `$E000` under the KERNAL, matrix in `$C000-$CFFF`. **No cost to the program area.** |

The VIC always reads RAM, never the KERNAL ROM, and CPU writes to
`$E000-$FFFF` always go to RAM. Only **reads** need the KERNAL switched out.
Clearing the bitmap is plain `memset(0xE000, 0, 8000)`. Setting pixels
(read-modify-write) runs with `SEI`, `$01 = $35` (BASIC and KERNAL out, I/O
in), then restores `$01` and the I/O flag (`spikes/rom-float/gfx/gfx.s`).
The real line routine should switch once per line, not per pixel. If
RESTORE (NMI) is pressed during such a window, the CPU fetches the NMI vector
from RAM at `$FFFA`. Task 12 should store a vector to an `RTI` at
`$FFFA/$FFFB` in `gfx_open`. That RAM lies past the bitmap's end at `$FF3F`,
so it is free.

### Register values (read back in VICE, `out/gfx-regs.txt`)

| Register | Bitmap on | Text (restore) | Notes |
|---|---|---|---|
| `$DD00` | `(x & $FC) \| 0` → read `$94` | `(x & $FC) \| 3` → read `$97` | bits 0–1 = inverted bank number |
| `$D018` | `$38` (read `$39`) | saved value; cc65 leaves `$17` (lower/upper charset), BASIC default `$15` | bits 4–7 = matrix offset/1K (`$CC00` → 3), bit 3 = bitmap at +`$2000` |
| `$D011` | `$3B` | `$1B` | bit 5 = bitmap mode |
| `$D016` | `$C8` (unchanged) | `$C8` | bit 4 = 0: hires, not multicolour |
| `$D020` | `1` (white border, optional) | saved | |
| matrix `$CC00-$CFE7` | fill with `$01` | n/a | high nibble = colour of 1-bits (black 0), low nibble = colour of 0-bits (white 1) |

Evidence: `out/gfx-bitmap.png` (frame, diagonals and a fan of lines, black on
white) and `out/gfx-text.png` and `gfx-textscreen.txt` after `gfx_close`.
The text printed before the drawing is still there, because bank 3 never
touches `$0400`. So the SPACE toggle in Task 15 is just these register writes.
No redraw is needed, although spec 10b(2) would allow one.

Option not tested: put the matrix in RAM under I/O (`$D000-$D3E7`,
`$D018 = $48`) to free `$CC00-$CFFF` for HIBSS (4 KB, `MAX_CODE 1024`).
Writing it needs `$01 = $34` with SEI. Adopt it only after a VICE check.

## 6. Weight stub: measured sizes

Stub (`spikes/rom-float/weight/`): `vm.c` (naive switch VM, 32 opcodes),
`vm2.c` (same opcodes in the cc65-friendly style of section 8), `editor.c`
(gap buffer, cursor movement, redraw to screen RAM, status line), `numw.c`
(the whole `num.h` API on `fp.s`), `fp.s`, and `wmain.c` (arrays, platform
stubs, benchmark). Built with `-O` and the proposed config.

### Code (bytes, from the maps)

| Module | `-O` | `-Oirs` |
|---|---|---|
| `vm.c` (naive VM) | 2,342 | 2,586 |
| `vm2.c` (cc65-friendly VM) | 2,030 (1,904 with asm copy) | 2,231 (2,156) |
| `editor.c` | 1,618 | 1,689 |
| `numw.c` (num.h on the ROM) | 837 | 942 |
| `fp.s` (with spike instrumentation) | 466 | |
| `copy5.s` | 30 | |
| cc65 runtime + libc pulled in | 4,057 | |
| … of which the printf family (`_printf`, `vsnprintf`, `vcprintf`, `sprintf`, `cprintf`) | 1,377 | |
| `wmain.c` (benchmark glue, not in the real app) | 1,569 | |

`-Oirs` makes the code 4–17% larger and the tuned VM only about 6% faster
on loops (section 8), so use plain `-O`.

### Draft limits do not fit

Draft arrays: code 2048×4 = 8192, stack 1024×5 = 5120, idents 64×22 = 1408,
source 16384, consts 64×5 = 320, strings 512, a total of 31,936 bytes.

- Stock config: BSS runs to `$B4C1`, inside `$A000-$BFFF`
  (`weight-draft-stock.map`).
- Proposed config, all arrays in ordinary BSS:
  `ld65: Warning: pl0-c64.cfg(28): Segment 'BSS' overflows memory area 'BSS' by 7362 bytes`
  (`out/weight-draft-pl0.txt`). That is **without** the compiler, lexer,
  turtle, graphics, I/O, picker and disk code.

### Proposed limits and placement (`weight3.prg`: vm2 + asm copy, `-O`)

| Segment | Range | Bytes |
|---|---|---|
| code + rodata + data | `$080D-$316E` | 10,594 |
| BSS (stack 512×5, consts 128×5, strings 1024, idents 64×22, logbuf, misc) | `$316F-$4C62` | 6,900 |
| **free before the C stack** | `$4C63-$97FF` | **19,357** |
| HIBSS: `code[768]` | `$C000-$CBFF` | 3,072 |
| BASICBSS: `source[8192]` | `$A000-$BFFF` | 8,192 |

Removing the stub-only parts (benchmark glue 1,569 + `logbuf` 1,024) leaves
about **21.9 KB** for everything not yet written.

### Budget for the full program (**estimates** for unwritten modules)

| Module (task) | Estimate |
|---|---|
| lexer (3) | 1.5 KB |
| compiler: parser + codegen, procedures and params, functions, turtle statements (5–9) | 6–8 KB |
| error table and texts (4) | 0.8 KB |
| turtle (9) | 1.0 KB |
| gfx C + asm line routine (12) | 1.0 KB |
| io (12) | 0.7 KB |
| editor beyond the stub: help, error line, confirm (14) | 1.0 KB |
| picker (15) | 0.8 KB |
| disk (15) | 1.2 KB |
| run flow and main (15) | 1.0 KB |
| help and menu texts | 0.5–1 KB |
| **total** | **about 15.5–18 KB** |

Margin: about **4–6 KB**, or about 5.5–7.5 KB if the printf family is avoided
(use `utoa` and the custom `num_format`). That is enough for the program,
but not for the examples on top. The real build must check the map at every
task that adds code: BSS must end at or below `$97FF`.

## 7. Proposed limits (for `src/core/limits.h`, Task 4)

| Limit | Value | Bytes | Where | Why |
|---|---|---|---|---|
| `MAX_CODE` | 768 | 3,072 (4-byte instr) | HIBSS `$C000` | A 600-byte example compiles to about 150–300 instructions. 768 allows user programs about 3× that. 1024 is possible if the matrix moves under I/O (untested). |
| `MAX_IDENTS` | 64 | 1,408 (22 B each with a 17-byte name) | BSS | Examples need about 10–20 names in scope. The draft value fits. |
| `MAX_STACK` | 512 | 2,560 | BSS | A frame is 2 link slots plus params and locals plus temps (about 8). That gives about 60 levels of recursion, enough for factorial, GCD, Koch depth 3–4 and the tree. Runaway recursion gives `E_STACK`. |
| `MAX_NEST` | 4 | | | as planned. It also bounds the display array (5 pointers). |
| `MAX_SOURCE` | 8192 | 8,192 | BASICBSS `$A000` | About 14 examples' worth, or 200+ lines of 40 columns. Without BASICBSS (section 3): 4096. |
| `MAX_CONSTS` | 128 | 640 | BSS | Literal pool, deduplicated. Literals cannot fit a 4-byte instruction. |
| `MAX_STRINGS` | 1024 | 1,024 | BSS | Total bytes of `'...'` text. |

Examples: 12 × about 600 bytes = 7.2 KB. **Do not embed them.** Store them on
the `.d64` as SEQ files (`SQUARE.PL0` and so on). The picker reads the
directory, or a small index file for the one-line descriptions, and loads the
chosen file into the source buffer. A stock 1541 reads about 400 B/s, so one
example loads in about 2 s. Task 15's `embed_examples.py` then produces disk
files and descriptions instead of a C array. The web demo already attaches
the `.d64`.

## 8. Go / no-go: the VM stays in C

Measured with `wmain.c` benchmarks: A = 702 straight-line LOD/STO, run 20×;
B = `while i < 500 do i := i + 1` (9 instructions per iteration);
C = same with `i := i*1 + 1` (11 per iteration). µs per P-code instruction,
averaged:

| Variant | A: LOD/STO | B: loop + ADD + LT | C: + MUL |
|---|---|---|---|
| `vm.c` naive (indices, `stack[i]` ×5 multiply, static-link walk, struct copy) `-O` | 1,077 | 1,190 | 1,295 |
| `vm2.c` pointers, pre-scaled offsets, display, C byte copy `-O` | 1,455 | 1,345 | 1,452 |
| same, `-Oirs` (copy in register vars) | 727 | 802 | 928 |
| `vm2.c` + 5-byte copy in asm (`copy5.s`) `-O` | **505** | **709** | **847** |
| same, `-Oirs` | 521 | 665 | 795 |
| minimal **assembly** dispatch loop (`vmasm.s`, jump table, LOD/STO only) | **210** | n/a | n/a |

Cycle counts confirmed in the VICE monitor: about 1,077 cycles between
dispatches in `vm.c`. The C byte-by-byte copy compiles to
`pushax`/`staspidx` per byte, about 80 cycles per byte. cc65 compiles
`switch` to a linear `CMP`/`BNE` chain.

**Decision: GO, the VM stays in C.** Reasons:
1. The Global Constraint "compiler and VM compile unchanged for host and
   C64" keeps the host tests (every example, without an emulator) valid for
   the VM. An assembly VM would be a second, untested implementation.
2. In real programs the ROM dominates: an ADD costs about 930 µs, MUL 1.9 ms,
   DIV 3.1 ms, SIN 27.5 ms. Assembly dispatch would make a numeric loop like
   B about 2–2.5× faster (estimate: 7 simple instructions × 210 µs + ROM add +
   compare). It would barely change the drawing examples, which are bound
   by ROM trig.
3. Memory does not force it: the margin in section 6 holds with the C VM
   (1.9 KB). An assembly VM would save about 0.6–1 KB (**estimate**).

Conditions (binding for Task 5/7/11):
- Write the VM in the `vm2.c` style: `num_t *` stack and frame pointers
  (never `stack[i]` with a 16-bit index, which costs a `mulax5` call); the
  compiler emits LOD/STO/RD operands as **byte offsets** (slot × 5); a display
  `num_t *disp[MAX_NEST+1]` indexed by the absolute declaration level instead
  of walking static links; no struct assignment of `num_t`.
- Add `num_copy(dst, src)` to the number interface. On the host it is a
  plain assignment; on the C64 it is an assembly routine (about 30 bytes, like
  `copy5.s`).
- Compile with `-O`.
- Fallback: if Task 15/16 measures an example over 30 s **and** the
  dispatch share is the cause, move only the hot opcodes (LIT, LOD, STO, JMP,
  JPC) into an assembly dispatcher that calls back into C for the rest.

### Step budget (affects Task 10)

At 0.5–0.85 ms per instruction (tuned C, ROM ops included), 30 s ≈
35,000–60,000 instructions for numeric code. Fewer if the code uses many DIVs
or printed numbers (FOUT ≈ 9 ms each). Drawing examples are limited by about
56 ms per `forward` with a changed heading. Set Task 10's provisional budget
to about **40,000 executed instructions** (not 2,000,000) and refine it in
Task 16 against VICE timings. Rough **estimates** for the planned examples:
PRIMES to 200 by trial division ≈ 20k instructions plus about 1,400 DIVs
(≈ 18 s); Koch depth 3 = 192 segments ≈ 11 s of trig. Both fit, but the
tuning in spec 10b(5) will matter.

## 9. Notes for later tasks

- cc65 C64 string literals are PETSCII by default (`'a'` in C is `$41`).
  Superseded by Task 12b: every C64 source is now compiled with the ASCII
  charmap (section "Text encoding" below).
- FOUT writes into `$0100-$010F`, the bottom of the 6502 stack page. That is
  harmless at cc65's stack depths, but copy the string out immediately.
- MOVMF rounds FAC before storing. Packed format: byte 0 = exponent
  (0 means the value is 0), byte 1 bit 7 = sign, bytes 1–4 = mantissa with
  an implied leading 1.
- `num_neg` can flip `b[1] & 0x80` in C when `b[0] != 0`. `num_is_zero` is
  `b[0] == 0`.
- The `fp_*` shims must stay below `$A000`, and so must every operand
  pointer (any `num_t` and any string given to `FIN`).

### Added by Task 12 (C64 graphics and I/O layers)

- **cc65 2.18 `-O` miscompiles a conditional expression** in an assignment
  when the false branch is a stack local: `buf[n++] = c == 0x65 ? 0x45 : c;`
  (with `c`, `n` locals and a pointer parameter) loads the false branch from
  the wrong stack offset (the parameter's). Without `-O` the code is right.
  Found in `io_read_num`; rewritten as `if (c == 0x65) c = 0x45;`.
  Reproducer: `tests/c64/cc65_bug_ternary.c` (build command and the wrong
  output in its header; not built by any target). Prefer
  `if` statements to `?:` on locals in C64 code, and test on the target.
- **Character codes** (Task 12 had them differ by module; resolved by
  Task 12b, see "Text encoding" below: ASCII everywhere inside the
  program).
- Text output (`src/c64/io_c64.s`) uses its own cursor; the KERNAL and
  cc65 `conio` cursors are not updated. Output is in the upper/lower case
  character set (`$D018 = $17`), wraps at column 40 (deferred: 40 characters and a new line take one row) and scrolls
  screen and colour RAM.
- `io_break` reads the keyboard matrix itself (`$DC00 = $7F`, `$DC01`
  bit 7) and the KERNAL buffer for code 3: 35 cycles per call when idle,
  one report per press, keyboard buffer emptied on a report. The KERNAL
  key scan ends with `$DC00 = $7F` (`$EB42`), so writing it is harmless.
- `gfx_line` keeps interrupts off for the whole line (KERNAL banked out):
  up to about 31 ms for a 320-pixel line. The CIA latches only one pending
  timer interrupt, so jiffy IRQs due in that window collapse into one: TI
  falls behind by 1-2 ticks per long line and the matching KERNAL key scans
  are skipped. Later code must not rely on TI across `gfx_line` calls (time
  drawings with the CIA timers instead). `gfx_open` points the RAM NMI
  vector `$FFFA` at an `RTI`.

### Text encoding (Task 12b, binding)

- **ASCII is the internal text encoding everywhere**: the source buffer,
  lexer (keywords), compiler, VM strings, error texts (`errors.c`),
  `num_format`/`num_parse` (exponent letter `E` = $45 written, `e`/`E`
  read, as on the host). PETSCII and screen codes exist only at the
  hardware boundaries: keyboard in (`io_read_num`, later the editor),
  screen out (`io_scr` in `io_c64.s`), disk files (Task 15).
- Mechanism: cc65 has no forced-include option, so the Makefile compiles
  each C64 C source through a generated one-line wrapper
  `build/c64/X.ascii.c` (`#include <ascii_charmap.h>` then
  `#include "<source>"`); this covers src/core, src/c64 and tests/c64
  alike. Assembly sources include `src/c64/ascii.inc` (an identity
  `.charmap` for 1..255), so `'E'` assembles to $45.
- Guard: `src/core/ascii.h` holds a typedef whose array size is negative
  unless `'a'`, `'z'`, `'A'`, `'E'`, `'\n'` and `'0'` are ASCII, so a
  PETSCII build of `lexer.c`, `errors.c`, `turtle.c`, `num_c64.c` or
  `io_c64.c` does not compile (written with XOR: cc65 2.18 rejects `==` in an
  array size and warns "Result of comparison is constant").
- Never use `<ctype.h>` in portable code: cc65's tables follow the target
  charset (`isalpha(0x61)` is false on the C64). `src/core/ascii.h` has
  `ASCII_IS_DIGIT/UPPER/LOWER/ALPHA` with explicit ranges (host-tested
  against `<ctype.h>` for all 256 values in `tests/test_lexer.c`).
- cc65 stdio cannot be used by C64 programs compiled this way: `printf`
  (libsrc, assembled for PETSCII) parses its format letters as PETSCII. The
  check programs print through `io_put_str` (`tests/c64/out.c`).
- Screen charset: the upper/lower case set (`$D018 = $17`); ASCII `A-Z`
  map to screen codes $41-$5A (upper case), `a-z` to 1-26, `@` to 0, the
  other printable ASCII as listed in `io_scr`; every byte >= $7F and every
  control code other than `\n` shows as a space.
- cc65 `-O` ternary bug audit (Task 12b): `?:` on stack locals was
  rewritten as `if` in `lexer.c` (identifier lower-casing, false branch a
  local: exactly the miscompiled pattern) and `clip.c` (`c = c0 ? c0 : c1`
  and the sign in `mul_div`). The remaining `?:` in src/core and src/c64
  pick between constants (`compiler.c` operator and turtle opcodes,
  `vm.c` error codes, `num_c64.c` saturation values and the error code from
  a static), which is not the miscompiled shape; they were left as they
  are.
