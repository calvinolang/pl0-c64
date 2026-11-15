; io_c64.s -- C64 text output and the RUN/STOP check (src/platform/io.h,
; io_c64.h); io_read_num is in io_c64.c.
;
; ---- output -------------------------------------------------------------
; Text goes straight to the 40x25 screen: screen RAM $0400, colour RAM
; $D800 in the current text colour ($0286, which cc65 textcolor() also
; sets). The cursor is our own: `pos` = row*40 + col (0..999, so a cursor
; never set by io_text_open is the top left corner) and `col`; the KERNAL
; and cc65 conio cursors are neither used nor updated. Wrapping is
; deferred: after the 40th character of a row the cursor stays in column 39
; with a wrap pending; the next character (or io_show) starts the next row
; first, while io_put_nl just goes to the next row, so 40 characters and a
; new line take one row, not two. (io_read_num shows its cursor with
; io_show, so during input the wrap happens as soon as column 39 is typed.)
; A new line below row 24 scrolls screen and colour RAM up one row (cc65
; conio would write past $07E7 instead: spike-results.md section 1).
; io_put_str takes ASCII, shown in the upper/lower case character set
; (io_scr below); io_text_open selects that set ($D018 |= 2: $15 -> $17;
; cc65's startup already leaves $17).
;
; ---- io_break -----------------------------------------------------------
; Has the user pressed RUN/STOP? (The C64 has no Esc key; "ESC" in the plan
; means RUN/STOP, and VICE maps the PC Esc key to it.) Polled by the VM on every backward jump,
; so the usual path is short: 35 cycles including JSR and RTS when no key
; is down and the keyboard buffer is empty (measured, tests/c64/io_check.c).
;
; Two sources, both non-blocking:
;   1. the keyboard matrix: CIA1 column 7 selected ($DC00 = $7F), row 7
;      ($DC01 bit 7) low = RUN/STOP down. Read directly, so it works with
;      interrupts on or off and does not depend on the KERNAL's STOP flag
;      ($91, which UDTIM leaves unchanged while a SHIFT key looks pressed).
;   2. the KERNAL keyboard buffer ($0277, count $C6): a RUN/STOP code 3
;      queued there (typed ahead, or injected by VICE `keybuf`).
;
; Contract: io_break returns 1 once per press, else 0.
;   - A matrix press is reported only if the key was seen up since the last
;     report (`armed`), so holding RUN/STOP stops one program only, and the
;     next call returns 0 while the key is still down. The first call after
;     the program starts arms it if the key is up.
;   - On a report the keyboard buffer is emptied (the code 3 the KERNAL
;     queued for this press, and any type-ahead, never reach the editor),
;     and LSTX ($C5) is set to the RUN/STOP key number 63, so the KERNAL scan
;     takes the key as already seen and queues no code 3 later for it.
;   - io_read_num, which reads RUN/STOP itself as code 3 (bad input),
;     clears `armed` the same way.


        .export _io_text_open, _io_text_close, _io_put_str, _io_put_nl
        .export _io_putc, _io_show, _io_back, _io_scr
        .export _io_break, _io_brk_armed
        .import _memmove, pushax
        .importzp ptr1, ptr3, ptr4

SCREEN  = $0400
COLRAM  = $D800
TEXTCOL = $0286         ; KERNAL current text colour
NDX     = $C6           ; number of keys in the KERNAL buffer
LSTX    = $C5           ; matrix number of the last key the KERNAL saw
KEYD    = $0277         ; KERNAL keyboard buffer (10 bytes)
STOPKEY = 63            ; RUN/STOP: column 7, row 7
STOPCODE = 3            ; its PETSCII code

.bss
pos:    .res 2          ; cursor: row*40 + col, 0..999
col:    .res 1          ; 0..39
wrap:   .res 1          ; nonzero: a character was written in column 39 and
                        ; the move to the next row is still pending
_io_brk_armed: .res 1   ; bit 7 set: a matrix press will be reported

.code
; void io_text_open(void): clear screen and colour RAM, cursor home
_io_text_open:
        lda     $D018
        ora     #$02
        sta     $D018
        ldx     #0
        stx     pos
        stx     pos+1
        stx     col
        stx     wrap
@clr:   lda     #$20
        sta     SCREEN,x
        sta     SCREEN+$100,x
        sta     SCREEN+$200,x
        sta     SCREEN+$2E8,x   ; last 256 bytes end at $07E7
        lda     TEXTCOL
        sta     COLRAM,x
        sta     COLRAM+$100,x
        sta     COLRAM+$200,x
        sta     COLRAM+$2E8,x
        inx
        bne     @clr
_io_text_close:
        rts

; void io_put_str(const char *s): ASCII; '\n' ($0A) is a new line
_io_put_str:
        sta     ptr4            ; ptr4: memmove (via io_put_nl) keeps it
        stx     ptr4+1
@next:  ldy     #0
        lda     (ptr4),y
        beq     @end
        cmp     #$0A
        beq     @nl
        jsr     _io_putc
        jmp     @adv
@nl:    jsr     _io_put_nl
@adv:   inc     ptr4
        bne     @next
        inc     ptr4+1
        bne     @next
@end:   rts

; void __fastcall__ io_putc(uint8_t c): ASCII c at the cursor, advance;
; in column 39 the cursor stays and the wrap is left pending
_io_putc:
        jsr     _io_scr
        jsr     _io_show        ; takes a pending wrap first
        lda     col
        cmp     #39
        bcs     @pend
        inc     col
        inc     pos
        bne     :+
        inc     pos+1
:       rts
@pend:  sta     wrap            ; A = 39: nonzero
        rts

; void __fastcall__ io_show(uint8_t sc): screen code sc at the cursor; a
; pending wrap is taken first (new row, scrolling if needed)
_io_show:
        ldx     wrap
        beq     @now
        pha
        jsr     _io_put_nl
        pla
@now:
        ldy     pos
        sty     ptr1
        sty     ptr3
        tax
        lda     pos+1           ; 0..3
        ora     #>SCREEN
        sta     ptr1+1
        lda     pos+1
        ora     #>COLRAM
        sta     ptr3+1
        txa
        ldy     #0
        sta     (ptr1),y
        lda     TEXTCOL
        sta     (ptr3),y
        rts

; void io_back(void): cursor one place back (io_read_num's DEL)
_io_back:
        lda     pos
        bne     :+
        dec     pos+1
:       dec     pos
        dec     col
        bpl     :+
        lda     #39
        sta     col
:       rts

; void io_put_nl(void): cursor to the start of the next row; below row 24
; the screen scrolls up one row
_io_put_nl:
        lda     #40
        sec
        sbc     col
        clc
        adc     pos
        sta     pos
        bcc     :+
        inc     pos+1
:       lda     #0
        sta     col
        sta     wrap
        lda     pos
        cmp     #<1000
        bne     @end
        lda     pos+1
        cmp     #>1000
        bne     @end
        lda     #<SCREEN        ; memmove(SCREEN, SCREEN + 40, 960)
        ldx     #>SCREEN
        jsr     pushax
        lda     #<(SCREEN+40)
        ldx     #>(SCREEN+40)
        jsr     pushax
        lda     #<960
        ldx     #>960
        jsr     _memmove
        lda     #<COLRAM        ; memmove(COLRAM, COLRAM + 40, 960)
        ldx     #>COLRAM
        jsr     pushax
        lda     #<(COLRAM+40)
        ldx     #>(COLRAM+40)
        jsr     pushax
        lda     #<960
        ldx     #>960
        jsr     _memmove
        ldx     #39             ; clear row 24
@clr:   lda     #$20
        sta     SCREEN+960,x
        lda     TEXTCOL
        sta     COLRAM+960,x
        dex
        bpl     @clr
        lda     #<960
        sta     pos
        lda     #>960
        sta     pos+1
@end:   rts

; int io_break(void)
_io_break:
        lda     #$7F
        sta     $DC00           ; select column 7
        lda     $DC01
        bpl     down            ; row 7 low: RUN/STOP is down
        sta     _io_brk_armed   ; key up (A bit 7 set): arm
        ldx     NDX
        bne     scan
        txa                     ; return 0
        rts
scan:   lda     KEYD-1,x        ; any code 3 among the queued keys?
        cmp     #STOPCODE
        beq     hit
        dex
        bne     scan
        txa
        rts
down:   bit     _io_brk_armed
        bmi     hit
        lda     #0              ; held since the last report
        tax
        rts
; LSTX first, then the buffer: if the jiffy IRQ's key scan runs between
; the two stores it already sees RUN/STOP as the last key and queues no
; code 3, so the buffer emptied next stays free of this press. (In the
; other order a scan in between could queue a code 3 after the buffer was
; emptied, and the editor would see it.)
hit:    lda     #STOPKEY
        sta     LSTX
        ldx     #0
        stx     _io_brk_armed
        stx     NDX             ; empty the keyboard buffer
        lda     #1
        rts

; uint8_t __fastcall__ io_scr(uint8_t c): ASCII -> screen code of the
; upper/lower case character set, for io_put_str (io_c64.c).
;   $20-$3F, A-Z $41-$5A        unchanged
;   @ $40                       0
;   a-z $61-$7A                 1..26
;   [ \ ] ^ _ `  $5B-$60        [ pound ] up-arrow, underscore graphic, '
;   { | } ~  $7B-$7E            [ | ] -   (no such C64 characters)
;   $C1-$DA                     A-Z: PETSCII upper case, as cc65 C literals
;                               and num_format's exponent letter 'E' = $C5
;   anything else               space (controls, DEL, other 8-bit codes)
_io_scr:
        cmp     #$C1
        bcc     @lo
        cmp     #$DB
        bcs     @sp
        and     #$7F            ; $C1-$DA -> $41-$5A
        rts
@lo:    cmp     #$7F
        bcs     @sp             ; DEL, $80-$C0
        cmp     #$7B
        bcs     @tab            ; { | } ~: table entries 6..9
        cmp     #$61
        bcc     @mid
        and     #$1F            ; a-z -> 1..26
        rts
@mid:   cmp     #$5B
        bcs     @tab            ; [ \ ] ^ _ `: table entries 0..5
        cmp     #$40
        bne     @ctl
        lda     #0
        rts
@ctl:   cmp     #$20
        bcs     @ok
@sp:    lda     #$20
@ok:    rts
@tab:   tax
        cpx     #$7B
        bcs     @t2
        lda     odd-$5B,x       ; $5B-$60 -> odd[0..5]
        rts
@t2:    lda     odd-$7B+6,x     ; $7B-$7E -> odd[6..9]
        rts

.rodata
odd:    .byte   27, 28, 29, 30, 100, 39, 27, 93, 29, 45
