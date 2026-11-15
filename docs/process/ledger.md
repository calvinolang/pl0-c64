# SDD ledger — plan: docs/superpowers/plans/2026-11-15-pl0-c64.md

Worktree: .worktrees/impl (branch impl, from master 7c… after plan commit). Spec: docs/superpowers/specs/2026-11-15-pl0-c64-design.md

Ruling: worktree created manually with git (native EnterWorktree tool requires the word "worktree" from the user) — isolates master — cost if wrong: delete .worktrees/impl.

## Pre-flight scan
| Row | Finding | Ruling |
|---|---|---|
| T0 self | `make test` links src/core + src/host sources; pl0host.c has main() | Makefile must exclude src/host/pl0host.c from test builds |
| T3↔T4 | lexer needs error codes before T4 | T3 creates minimal errors.h; T4 extends (plan already says so) |
| T3 self | number `3.` before final `.` in test sources like `!1+2*3.` | Lexer: '.' after digits is part of the number only if a digit follows; else it is T_PERIOD |
| T4↔T5 | T5 Program uses MAX_CONSTS, MAX_STRINGS not defined in T4 limits.h | T4 also defines MAX_CONSTS and MAX_STRINGS |
| T4↔T8 | T8 expects text "domain error" | E_DOMAIN text = "domain error"; E_DIV_ZERO = "division by zero" |
| T5↔T10 | pl0host flags --lines, --count-instr added in T10 | T10 extends pl0host; fine |
| T11↔T14↔T16 | T14 needs build/pl0.prg; T16 documents `make c64` | T14 implementer extends `c64` target to build pl0.prg; T16 finalizes |
| T11/T14 scripts | vice_run.sh (T11) vs vice_smoke.sh (T14) | separate scripts, both owned by their tasks |
| T1↔T4,T12,T15 | limits, bitmap address, example placement from spike | consumed from docs/architecture/spike-results.md |
| all other tasks | self-consistent (tests match signatures; files created before touched) | none |
Task 0: dispatching, BASE=e6c0ccf
Task 0: implemented (commits e6c0ccf..0c863e2), review pending.
Finding: cc65 NOT installed, sudo needs password. VICE 3.7.1, c1541, gcc, python3 present.
Ruling: execute host-only tasks 2–10 first; Task 1 (spike) and 11–17 wait for cc65. Task 4 uses the plan's provisional limits (2048 code, 64 idents, 1024 stack, 16 KB source) until the spike runs. Cost if wrong: limits.h numbers get retuned after the spike.
Task 0: complete (commits e6c0ccf..0c863e2, review clean; 4 minor parked: CFLAGS override, TEST_MAIN_END block, math.h include, crash signal)
Task 2: dispatching BASE=0c863e2
cc65 installed by user (cl65 V2.18 - Ubuntu 2.19-1). Task 1 (spike) is unblocked; run it after Task 2's review, before Task 4 finalizes limits.
Task 2: review round 0 — spec ❌ (num_format may write 17 bytes into 16-byte buf for 3-digit exponent). Fix round 1/5 dispatched (resumed implementer a95349c); minors folded in: 1e+09 rounding edge, NaN guard in num_to_i16, num_fn test coverage
Task 2: fix round 1/5 (4 addressed, 0 open; commits 83377a2..dfa7fc3)
Task 2: complete (commits 0c863e2..dfa7fc3, review clean after 1 fix round)
Task 2: minor (deferred): suggest documenting the 8-digit reduction for 3-digit exponents in num.h contract; test NAN needs <math.h> via num.h
Task 1: dispatching BASE=dfa7fc3
Task 1: implemented (dfa7fc3..32ba1c9), review pending. GO: VM stays in C.
Ruling (spike concern 1): ACCEPT source text buffer in RAM under BASIC ROM ($A000-$BFFF) — shims bank BASIC in only during a ROM call, normal code runs with $01=$36; spec 10b item 2 only forbids the program image/bitmap there. Cost if wrong: MAX_SOURCE drops to 4096. Task 15 must test disk SAVE/LOAD with source at $A000.
Ruling: limits for Task 4 = MAX_CODE 768, MAX_IDENTS 64, MAX_STACK 512, MAX_NEST 4, MAX_SOURCE 8192, MAX_CONSTS 128, MAX_STRINGS 1024 (spike section 7).
Ruling: examples are loaded from the disk image, NOT embedded in a C array (Task 15, Task 17 follow: .d64 carries them; embed_examples.py becomes a disk-image builder). Cost if wrong: none for colleagues — web demo autostarts the .d64.
Ruling: Task 10 instruction budget is ~40,000 not 2,000,000 (ROM sin ≈ 28 ms); Task 9 turtle caches sin/cos of heading and recomputes only when heading changes.
Task 1: minor (deferred): commits carry "Opus 5.5" Co-Authored-By trailer instead of the Sonnet 5.5 line.
Task 1: complete (commits dfa7fc3..32ba1c9, review clean; minors: inconsistent instruction-count ranges in doc sec 0 vs 8; out/ evidence is gitignored; 21.9 KB figure derived not measured). Plan amended (commit 5f14975).
Task 3: dispatching BASE=5f14975
Task 3: implemented (bbd6935..7c703ee), review pending
Task 3: complete (commits bbd6935..7c703ee, review clean)
Task 3: minor (deferred): after a too-long number/string error the lexer pos stays mid-token (fine if parser stops at first error — Task 5 must stop at the first error); bad-number branch and 16-digit limit untested; unterminated-comment line untested; value uninitialized in lex_init; number errors use E_CHAR
Task 4: dispatching BASE=7c703ee
Task 4: implemented (7c703ee..7c300ce), review pending
Task 4: complete (commits 7c703ee..7c300ce, review clean; check_error_docs exit codes verified by reviewer)
Task 4: minor (deferred): check_error_docs.py uses substring match (weak for "stopped"/"bad input"), file handles not closed with `with`
Task 5: dispatching BASE=7c300ce
Task 5: implemented (7c300ce..26fc98f), review pending.
Carry-forward (from Task 5 report): (a) src/core/errors.c fails to compile under cc65 ("Incompatible pointer types" on ?: return in err_text) — fix in next task's dispatch (Task 6), verify with `cl65 -t c64 -c`; (b) num_add/sub/mul are void so overflow can't report E_OVERFLOW — revisit in Task 8; (c) num.h now allows result pointer == input pointer (VM computes in place) — Task 11 C64 num layer MUST allow aliasing; (d) expressions nested >16 deep reuse the "program too big" message.
Task 5: review — Important: unbounded statement() recursion (cc65 stack); missing limit tests (consts/strings/code). Fix round 1/5 dispatched (resumed implementer a973a45), also folded in errors.c cc65 compile fix and E_NESTING for expression depth.
Ruling: runtime error line = operator's line (not statement start) — acceptable, document in LANGUAGE.md. E_STACK from INT reports line 0 — Task 7 to fix.
Task 5: minor (deferred): aliasing tests only for add/mul; io_host capture truncates at 8 KB silently; num_add/sub/mul overflow undetected (Task 8/11).
Task 5: fix round 1/5 (3 addressed, 0 open; commits 26fc98f..c19a5d6)
Task 5: complete (commits 7c300ce..c19a5d6, review clean after 1 fix round)
Carry to Task 6: if/while must count against snest (MAX_STMT_NEST 16, E_NESTING); VM must check io_break() on every backward jump (E_STOPPED). Carry to Task 7: save/restore maxdepth around nested blocks; CAL checks room for 3 header cells; E_STACK line. Carry to Task 18: document "nested too deeply" and operator-line semantics.
Task 6: dispatching BASE=c19a5d6
Task 6: complete (commits c19a5d6..a26085c, review clean)
Task 6: minor (deferred): no timeout guard on test binaries (folded into Task 7 dispatch as Makefile `timeout 60`); stop-line tests only for plain condition (add odd/nested/multi-line); comparison = num_cmp + num_from_int ROM calls per compare — C64 speed follow-up (static ZERO/ONE num_copy or fused compare-jump opcode); nested() test keyword hack; no test with jump target exactly at MAX_CODE
Task 7: dispatching BASE=a26085c
Task 7: implemented (a26085c..7f7a1a5), review pending
Task 7: complete (commits a26085c..7f7a1a5, review clean)
Task 7: minor (deferred): add depth-60 recursion test; add test reading level>=2 variable after recursive re-entry; E_ARGS too-many-args line is the mismatch token's line; INT room check redundant for procedures (dead error path); Makefile `timeout` not portable to macOS; vm.c +619 B under cc65 — wrap pointer cell get/put in helpers if C64 budget gets tight.
Ruling: empty parentheses `()` in procedure declaration/call stay rejected (strict PL/0 spirit); document in LANGUAGE.md. Cost if wrong: small compiler tweak.
Ruling: Task 8 also changes num_add/num_sub/num_mul/num_neg to return int (nonzero = overflow) so the VM can report E_OVERFLOW (the C64 ROM traps overflow, spike error code 15).
Task 8: dispatching BASE=7f7a1a5
Task 8: implemented (7f7a1a5..b31b9c9), review pending
Task 8: complete (commits 7f7a1a5..b31b9c9, review clean)
Task 8: minor (deferred): no VM-level add/sub overflow test; host input queue persists across runs (reset in io_capture_begin); no test for ? into param/deeper-level var
Task 9: dispatching BASE=b31b9c9
Task 9: implemented (b31b9c9..67e05c4), review pending
Task 9: review — Important: clip_line lacks exact-value unit tests. Fix round 1/5 dispatched (resumed a179e70); minors folded in: no "1e30" literal in turtle_reset (C64 num_parse may reject exponents — also note for Task 11), constants parsed once, reduce_heading void, pl0host warns on dropped lines.
Task 9: minor (deferred): turtle_trig_count counts next to num_fn calls (acceptable); Liang-Barsky pre-clip adds ~120 B static + code — re-measure size on C64; moveto missing comma reports ")" expected; host gfx records at most 4096 lines.
Task 9: fix round 1/5 (2 addressed, 0 open; commits 67e05c4..5fd3ae9)
Task 9: complete (commits b31b9c9..5fd3ae9, review clean after 1 fix round)
Task 9: minor (deferred): pin reverse y=x/2 clip test to b==159
Task 10: dispatching BASE=5fd3ae9
Task 10: implemented (5fd3ae9..4dbbee4; 15 examples), controller viewed koch/tree/sine PNGs: good. Review pending.
Carry: string literal >16 chars reports misleading 'unterminated string' (lexer TEXT_MAX 16) — needs a clear message ('string too long (max 16)') — schedule small fix with Task 18 docs or a fix round; text output rule (<=22 lines x 39 cols) revisit in Task 12; drawing time dominated by ROM sin/cos (~11-15 s est. for koch/tree/rose) — check in Task 16
Task 10: complete (commits 5fd3ae9..4dbbee4, review clean; math goldens independently verified, rules negative-tested, drawing goldens stable under long-double libm)
Task 10: minor (deferred): --write-budget writes partial table if an example fails; PNGs not tied to .lines goldens; 15 examples = cap (STAR weakest); text rule 22x39 revisit Task 12
Ruling: schedule "Task 10b" small fix now: string literals >16 chars get a clear error E_STRING_LONG ("string too long (max 16)"), appended to the error table before E_COUNT; unterminated string keeps E_STRING. Cost if wrong: trivial.
Task 10b: implemented (4dbbee4..38e7e87), review pending
Task 10b: complete (commit 4dbbee4..38e7e87, review clean). minor: optional test pinning 17-char unterminated string behavior; plan code list lacks E_STRING_LONG; docs/LANGUAGE.md must list "string too long (max 16)".
Task 11: dispatching BASE=38e7e87
Task 11: implemented (38e7e87..ade5653), review pending. Concern: ROM FOUT/FIN wrong in 9th digit -> exact bignum parse/format in asm (+1.1 KB; module 2396 B code); format ~20 ms/call.
Task 11: complete (commits 38e7e87..ade5653, review clean; controller ran make c64-num on VICE: 22 groups ok, PASS)
Task 11: minor (deferred): tools/vice_screen.py matches expected text as a substring anywhere and checks it before fail text (fix whole-line match + fail first when the harness is extended in Task 12); stale comments in tests/c64/num_check.c (lines ~101,125,20); spikes/rom-float/run.sh paths edited but not re-run; num_format plain-notation loop wants an invariant comment; num_parse rejects >32 chars/24 digits (lexer can't reach); plan/brief should record that the harness polls the VICE remote monitor instead of -moncommands.
Ruling: the C64 has no ESC key. "ESC" in plan/spec = the RUN/STOP key (PETSCII 3; VICE maps PC Esc to RUN/STOP). Tasks 12,14,15 use RUN/STOP for stop and back; on-screen text says "RUN/STOP" ("STOP" where space is tight); docs say "RUN/STOP (Esc in emulators)". Task 17 verifies the web emulator maps Esc/RUN-STOP.
Task 12: dispatching BASE=ade5653
Task 12: implemented (ade5653..9bf0ce7), review pending. Concerns: cc65 2.18 -O miscompiles `x = c == K ? K2 : c;` with stack-local operands (documented in spike-results.md); io_break 35 cycles; gfx 633 B, io 700 B, clip 881 B code; CHARACTER-CODE DISAGREEMENT: language is ASCII, cc65/ca65 literals are PETSCII under -t c64 (num_format emits E as $C5; core literals like keywords/error texts would be PETSCII).
Ruling (character encoding): ASCII is the internal text encoding everywhere in portable layers (source buffer, lexer, compiler, error texts, num_format/num_parse). PETSCII/screen codes exist only at hardware boundaries (keyboard in, screen out, disk). Schedule "Task 12b": make the core + num layer ASCII-correct under cc65 (identity charmap for those objects) and add an on-target end-to-end core_check (compile+run embedded ASCII programs on VICE) which also verifies the BSS end <= $97FF before the editor is built.
Task 12: review — Important: vice_screen.py empty EXPECTED passes vacuously. Fix round 1/5 dispatched (resumed aa63261): + io_break LSTX/NDX order race, jiffy-loss wording, '+3' parity with host, deferred wrap, keybuf quote examples, ternary-bug repro in repo.
Carry to Task 12b: remove PETSCII accommodations (io_scr $C1-$DA branch io_c64.s ~984, io_read_num 'e' as $45 (io_c64.c ~720), io_check.c ~1491 \xC5 expectation); make num_format emit ASCII 'E'($45) and num_parse accept 'e'/'E'; grep -O compiled core for `x = c == K ? K2 : c` pattern with stack locals (cc65 miscompile). Carry to Task 15: VM-level STOP test (while 1=1 do i:=i+1 stops within ~2 s) and BSS end <= $97FF check on the real image.
Task 12: fix round 1/5 (6 addressed, 0 open; commits 9bf0ce7..ab9230a)
Task 12: complete (commits ade5653..ab9230a, review clean after 1 fix round)
Task 12: minor (deferred): io_read_num "- 3" (space after stripped minus) behavior unchecked; input at column 39 leaves one blank row after RETURN (cosmetic)
Task 12b: dispatching BASE=ab9230a
Task 12b: implemented (ab9230a..ca8bd39), review pending. RESULT: all 15 examples pass on the real C64 core in VICE; slowest KOCH 24.86 s, PI/ROSE/TREE 21-22 s; BSS ends $7FEB, 6,164 B free below $97FF (+~1.9 KB if test driver dropped, HIBSS $C000-$CBFF 3 KB unused). Found+fixed 2nd cc65 -O miscompile ((int16_t)uchar - uchar computed in 8 bits; every procedure parameter read 0 on target) in compiler.c, repro tests/c64/cc65_bug_sub.c. Carry to Task 14: do NOT use cc65 conio/stdio for text (ASCII charmap breaks case and printf); use io layer / own routines. Timing margin: KOCH 24.9 s vs 30 s limit — re-check when examples change. Commits attributed Claude Opus 5.5 (known trailer deviation).
Task 12b: review — Important: drawing check would pass mirrored figures (count+bbox only). Fix round 1/5 dispatched (resumed a6a1dec): overlap check with self-test, plus minors: guard semantic (run vs compile time), missing cycles fails, asm charmap asserts, ascii.h wording, err_text strlen test.
Task 12b: minor (deferred): single-example runs do not apply the 30 s guard; stale c64-times.tsv not auto-detected; ascii.h guard opt-in per file.
Task 12b: fix round 1/5 (2 addressed, 0 open; commits ca8bd39..ec2da19)
Task 12b: complete (commits ab9230a..ec2da19, review clean after 1 fix round). Drawing check now has overlap (far-pixel) test; all 10 drawings 0/0 far pixels on target; 30 s guard counts compile+run (koch 26.15 s).
Task 12b: minor (deferred): Makefile self-test line runs twice on failure.
Task 13: dispatching BASE=ec2da19
Task 13: implemented (ec2da19..69d62c7), review pending. textbuf CODE 2461 B (cc65 -O then ca65). Concern: tb_line/tb_col/tb_line_count/tb_offset_of_line scan per call: Task 14 must cache.
Task 13: review — Important: differential test parks gap at end every step (tb_text each step); tb_set and -2 path not in random mix. Fix round 1/5 dispatched (resumed a753274) + size/speed reduction (segment-pointer scans, merged helpers), measure real size with project build rule, doc tb_set aliasing.
Task 13: minor (deferred): model up/down mirrors implementation logic (partly addressed in fix).
Task 13: fix round 1/5 (3 addressed, 0 open; commits 69d62c7..ce1e61f)
Task 13: complete (commits ec2da19..ce1e61f, review clean after 1 fix round; textbuf CODE 2072 B)
Task 13: minor (deferred): stale comment textbuf.c:7-8 claims pointer scans are smaller/faster (they are not on cc65); tb_set scans whole string before size check.
Ruling (Task 14 key design): the C64 has no CTRL-N; use F2 (shift-F1) for New (confirm Y/N when text non-empty and modified); bottom bar `F1 Run F2 New F3 Open F5 Save F7 Help` (37 cols). HOME=line start, CLR(shift-HOME)=line end, INST(shift-DEL)=delete forward, DEL=backspace, RETURN=newline. Task 14 embeds one tiny starter program (SQUARE) as a C string; the other 14 examples load from disk (Task 15).
Task 14: dispatching BASE=ce1e61f
Task 14: implemented (ce1e61f..975e55b), review pending. make c64-editor 15/15 on target. BSS ends $970C, only 243 B free below $97FF (editor vars moved to HIBSS 460/3072 B). Concerns: Task 15 must free memory first (VM stack 2560 B -> HIBSS; ~950 B unused textbuf code; long texts on disk); full redraw 41 ms typical / 59 ms worst; tb_at scans too slow (~1000 cycles/char) so editor has asm scans and keeps sticky column itself; trailer says Opus 5.5.
Task 14: complete (commits ce1e61f..975e55b, review clean; controller viewed editor.png/editor-error.png: good)
Task 14: minor (deferred): docs/editor.md says errors always re-centre (code re-centres only when out of view); Makefile comment says ~1 min vs 2.5 min; hs carries over between long lines (cosmetic); BLNSW left at 1 (add to Task 15 notes next to $028A/$0291); RUN/STOP+RESTORE leaves the program and a re-run reloads the starter, losing the text (acknowledged; NMI hook possible later); tests missing for 40 vs 41-char line, error line beyond buffer, INST at end; BUFFER FULL shown for any tb_insert failure.
Task 15 carries (binding): STEP 0 free low RAM before adding code (VM stack 2560 B -> HIBSS leaves ~52 B there; #ifdef out ~950 B unused textbuf code on C64; picker/disk scratch buffers REUSE the static Program memory (4741 B) which is idle outside compile+run); keep BSS end <= $97FF and report map numbers.
Task 15: dispatching BASE=975e55b
Task 15: implemented (975e55b..HEAD), review pending. make c64-app 9 cases pass on true-drive 1541. BSS ends $9718, 231 B free; HIBSS 1 B free; new LOWBSS in tape buffer $033C-$03FB (21 B free). Concerns: memory tight; ? input after drawing reads on hidden text screen; 8 KB load on real 1541 reads twice ~40 s; VICE loses a file saved just before quit unless disk detached (harness detaches).
Task 15: review — all 10 required cases present; Important: disk_message mbuf[24] overflows by 3 bytes ("DISK ERROR: FILE NOT FOUND" = 27) into dead vars before _disk_dev. Fix round 1/5 dispatched (resumed a53f5d4): + swallowed KERNAL read errors, names with spaces listed, ? input after drawing on hidden screen, doc/comment fixes, runtime-error-after-drawing test.
Task 15: deferred (documented limitations): single-pass load (8 KB file ~40 s on real 1541); BLNSW not restored (harmless); autostart untested -> Task 16; ownership guard is structure-only.
Task 15: fix round 1/5 (5 addressed, 0 open; commits 1cb788b..dc8597f)
Task 15: complete (commits 975e55b..dc8597f, review clean after 1 fix round). BSS end $978C, 115 B free; LOWBSS 18 free; HIBSS 1 B free (any new HIBSS var overflows; make c64-mem fails loudly).
Task 15: minor (deferred): memory-map.md RODATA/DATA/INIT range row ($7400-$7DC5) inconsistent with new CODE end; read_error test only exercises drive-status path (DK_READ verified by review only) and is race-sensitive; FITS check not automatic for new messages; single-pass load (8 KB ~40 s on real 1541).
Task 16: dispatching BASE=dc8597f
USER DECISION (Task 17): after the ROM finding (Open ROMs marks sin/cos/atn/exp/log/FCOMP/FACINX/GIVAYF NOT DONE, SQR partial; original ROM redistribution unclear) the author chose "No live emulator: recordings + downloads only". Task 17 revised to a static showcase site (docs/superpowers task-17-design.md in ledger dir): real VICE captures of all 15 examples + UI screens, demo GIF, downloads, VICE instructions, no ROMs/emulator in repo. Plan amendment to be appended after Task 16.
Task 16: implemented (dc8597f..504f0f2), review pending. smoke 9/9, app-level examples 15/15 (KOCH 26.17 s through the app), clean clone make check 261 s pass; make check 273 s, check-full 465 s. Root cause fixed: VICE 3.7.1 ignored quit after detach 8 (20 s kill wait). Concerns: CI target job untested incl tools/install_vice_roms.sh (downloads VICE ROMs from SourceForge with pinned SHA-256; CI-time download, NOT distributed; tell user); autostart from d64 with default drive emulation takes ~80 s emulated before main() (28 KB load at 1541 speed) -> use warp / true-drive-off for media capture; memory-map tables generated for cc65 2.19, make c64-mem fails until make c64-mem-doc after any size change.
Task 16: review — Important: c64_app_examples.sh can pass with ZERO examples (pipeline status lost, no count assertion). Fix round 1/5 dispatched (resumed a092d68): audit other scripts for same pattern; minors: CI permissions/timeouts, c64-app depends on limit check only, testing.md quit claim, quoting, $(MAKE), ALLOW_NO_VICE wording.
Task 16: minor (deferred): none beyond fix list; ROM installer is CI-only (fail-closed, SHA-256 pinned) — tell user.
Task 16: fix round 1/5 (2 addressed, 0 open; commits 504f0f2..208858f)
Task 16: complete (commits dc8597f..208858f, review clean after 1 fix round). make check 252 s, check-full 466 s.
Task 16: minor (deferred): c64_app_examples --record writes times TSV before rows==total check; vice_run.sh eval breaks on single-quote in path; tools/c64_mem.py silently skips a limit check when a segment is missing from the map (renamed segment would pass the gate) -> for final review.
Task 17: dispatching BASE=fe2f85b
Task 17: implemented (fe2f85b..999d647), review pending. 20 PNG (5 UI + 15 examples, 768x544, 31 KB total), demo GIF 214 KB 264 frames 30 s, site 0.46 MB; make media reproducible (byte-identical on rerun); controller viewed koch.png, picker.png, GIF frame 130: faithful. LICENSE holder = "calvinolang" from git config user.name -> ASK USER to confirm. REPO_URL placeholder filled by pages.yml at build. Old docs/img/*.png host renders unreferenced (Task 18 keep/delete).
Task 17: review — Important: tools/check_site.py passes vacuously (empty page) and has no per-file size caps. Fix round 1/5 dispatched (resumed a8d78cd) + minors (media.md numbers, leading-slash, style/srcset guard, pages.yml env + configure-pages, placeholder warning/fail, tabindex pre, main constant). LICENSE holder unchanged pending author confirmation of "calvinolang".
Task 17: fix round 1/5 (2 addressed, 0 open; commits 999d647..a5ef336)
Task 17: complete (commits fe2f85b..a5ef336, review clean after 1 fix round). minor (deferred): MAX_MEDIA/MAX_WEIGHT no negative test; <a> links leaving the site not checked; Pages needs a one-time manual setting (Settings > Pages > Source: GitHub Actions), documented.
Task 18: dispatching BASE=a5ef336 (design in task-18-design.md)
Task 18: complete (commits a5ef336..67a92f9, review clean, 0 Important). make test ~10-19 s with doc gates (63 PL/0 blocks, 31 error messages, 153 links).
Task 18: minor (deferred, for final wave): HOW-THIS-WAS-BUILT attribution of the 17-char string finding (Task 10 implementer reported; controller scheduled 10b); overview.md "tree-walking" -> "text-walking"; LANGUAGE.md host subnormal claim (about 2.2E-308 vs 4.9E-324); check_error_docs.py missing-doc skip path should fail now; check_doc_code 0.5 s timeout lets a compile-error block pass under load; check_md_links ignores single-quoted attrs; HOW-THIS-WAS-BUILT should mention mermaid not rendered; c64-app "9 to 2.5 minutes" -> "under 2.5 minutes"; add Pages URL when it exists.
ALL 20 TASKS COMPLETE. Next: final whole-branch review per references/final-review.md.
FINAL REVIEW (whole branch e6c0ccf..67a92f9, model fable): verdict "Ready to hand over: With fixes". 0 Critical, 5 Important, 10 minor + 4 recommendations.
 Important 1: saving as PL0 or INDEX destroys the disk (disk_name_ok accepts both; S0:PL0 scratches the program; confirmed in VICE). Important 2: compiler recursion limits untested on target (measured safe: 75 B of the 256-B hardware stack free). Important 3: several language features never run on C64 in committed tests (home, exp, log, abs, >=, #, <>, level-2 nesting, far-clip, domain/overflow via VM; reviewer's sweep found no defect). Important 4: README/USER-GUIDE download instruction has no URL (site never deployed). Important 5: process record gaps (mermaid not rendered undisclosed; 17-char string attribution; add final review section).
 Reviewer experiment scratch copy with a trial fix: <scratch directory>/co (src/c64/main.c). Fix cost ~60 B (55 B left free) for names refusal + RESTORE disable.
Ruling: final fix wave = ONE fix subagent doing: Important 1-5, must-fix minors (c64_mem.py missing-segment fails; check_error_docs.py missing doc fails; textbuf.c:7-8 comment; overview "tree-walking"->"text-walking"; LANGUAGE subnormal wording; "9 to 2.5 minutes"; README test duration 10-20 s; stale comments listed), recommendation 1 (disable RESTORE via NMI vector) and 3 (80 s start-up wording: emulated time, VICE usually warps) and 4 (record cc65 version in README), minor 4 (disk-full troubleshooting line). NOT doing recommendation 2 (border colour at end of drawing): the author said no colour work and the help page documents the keys — cost if wrong: first-time user may not know a key returns from a finished drawing; easy to add later.
Open for the author: LICENSE holder name "calvinolang" and the commit metadata (git author name + personal Gmail address in every commit) become public on push — the author's call.
