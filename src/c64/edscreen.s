; edscreen.s -- gap buffer scans and screen drawing for the C64 editor
; (src/c64/editor.c).
;
; Everything here works on the editor's TextBuf `ed_tb` (editor.c checks
; the field offsets below at compile time) with logical offsets, the way
; textbuf.h counts them; the gap is skipped inside. The same scans in cc65
; C were either slow (through tb_at: about 1000 cycles a character) or
; large (pointer loops over the two segments: about 650 bytes); these cost
; about 40 cycles a character for ed_bol/ed_eol and about 29 for each
; character ed_rows puts on the screen.
; Physical layout: [buf, g0) text before the gap, [g0, g1) the gap,
; [g1, gend) the rest, with g0 = buf+gs, g1 = buf+gs+cap-len, gend = buf+cap.
; Text is ASCII; ed_tbl maps it to screen codes (filled by editor.c from
; io_scr in io_c64.s, the one place that defines the mapping).
; Zero page: ptr1 (text), ptr2 (screen), tmp1; all caller-saved in cc65.

        .include "ascii.inc"   ; ASCII character constants
        .assert 'E' = $45 && 'a' = $61, error, "text must be ASCII: include ascii.inc"
        .export _ed_rows, _ed_put, _ed_bol, _ed_eol
        .export _ed_scr, _ed_hs, _ed_rev, _ed_tbl, _ed_n, _ed_cur
        .import _ed_tb
        .importzp ptr1, ptr2, tmp1

NL      = $0A
TB_BUF  = _ed_tb+0      ; TextBuf fields (textbuf.h)
TB_CAP  = _ed_tb+2
TB_LEN  = _ed_tb+4
TB_GS   = _ed_tb+8

.segment "HIBSS"         ; cleared by main() (crt0 does not)
_ed_scr: .res 2         ; screen address of the row to draw
_ed_hs:  .res 2         ; ed_rows: characters hidden on the cursor's row
_ed_n:   .res 1         ; ed_rows: rows to draw
_ed_cur: .res 1         ; ed_rows: index of the cursor's row, $FF: none
_ed_rev: .res 1         ; ed_put: $80 = reverse video, else 0
_ed_tbl: .res 256       ; ASCII -> screen code
g0:     .res 2
g1:     .res 2
gend:   .res 2
lim:    .res 2          ; end of the current segment
cnt:    .res 2          ; characters still to skip
col:    .res 1          ; next screen column, 0..40
more:   .res 1          ; nonzero: the line goes on right of the window
n:      .res 1          ; length of the current chunk
hsrow:  .res 2          ; characters hidden on the row being drawn
row:    .res 1

.code
; g0, g1, gend from ed_tb; then ptr1 = physical address of the logical
; offset in A/X (offsets >= gs are after the gap: gs itself maps to g1)
phys:   pha
        txa
        pha
        clc
        lda     TB_BUF
        adc     TB_GS
        sta     g0
        lda     TB_BUF+1
        adc     TB_GS+1
        sta     g0+1
        sec                     ; g1 = g0 + cap - len
        lda     TB_CAP
        sbc     TB_LEN
        tax
        lda     TB_CAP+1
        sbc     TB_LEN+1
        tay
        txa
        clc
        adc     g0
        sta     g1
        tya
        adc     g0+1
        sta     g1+1
        clc
        lda     TB_BUF
        adc     TB_CAP
        sta     gend
        lda     TB_BUF+1
        adc     TB_CAP+1
        sta     gend+1
        pla                     ; offset: hi
        tax
        pla                     ; lo
        cmp     TB_GS
        pha
        txa
        sbc     TB_GS+1
        pla
        bcs     @after
        clc                     ; before the gap: buf + off
        adc     TB_BUF
        sta     ptr1
        txa
        adc     TB_BUF+1
        sta     ptr1+1
        rts
@after: sec                     ; g1 + (off - gs)
        sbc     TB_GS
        tay
        txa
        sbc     TB_GS+1
        tax
        tya
        clc
        adc     g1
        sta     ptr1
        txa
        adc     g1+1
        sta     ptr1+1
        rts

; A/X = logical offset of the physical address ptr1 (after phys)
logical:
        lda     ptr1
        cmp     g1
        lda     ptr1+1
        sbc     g1+1
        bcs     @after
        sec                     ; ptr1 - buf
        lda     ptr1
        sbc     TB_BUF
        tay
        lda     ptr1+1
        sbc     TB_BUF+1
        tax
        tya
        rts
@after: sec                     ; ptr1 - g1 + gs
        lda     ptr1
        sbc     g1
        tay
        lda     ptr1+1
        sbc     g1+1
        tax
        tya
        clc
        adc     TB_GS
        pha
        txa
        adc     TB_GS+1
        tax
        pla
        rts

; uint16_t __fastcall__ ed_bol(uint16_t off): start of the line holding off
_ed_bol:
        jsr     phys
        ldy     #0
@loop:  lda     ptr1            ; at g1: go on before the gap
        cmp     g1
        bne     @1
        lda     ptr1+1
        cmp     g1+1
        bne     @1
        lda     g0
        sta     ptr1
        lda     g0+1
        sta     ptr1+1
@1:     lda     ptr1            ; at buf: line 1
        cmp     TB_BUF
        bne     @2
        lda     ptr1+1
        cmp     TB_BUF+1
        beq     @done
@2:     lda     ptr1            ; look at the character before
        bne     @3
        dec     ptr1+1
@3:     dec     ptr1
        lda     (ptr1),y
        cmp     #NL
        bne     @loop
        inc     ptr1            ; the line starts after it
        bne     @done
        inc     ptr1+1
@done:  jmp     logical

; uint16_t __fastcall__ ed_eol(uint16_t off): the '\n' ending the line
; holding off, or len
_ed_eol:
        jsr     phys
        ldy     #0
@loop:  lda     ptr1            ; at g0: go on after the gap
        cmp     g0
        bne     @1
        lda     ptr1+1
        cmp     g0+1
        bne     @1
        lda     g1
        sta     ptr1
        lda     g1+1
        sta     ptr1+1
@1:     lda     ptr1            ; at the end of the text
        cmp     gend
        bne     @2
        lda     ptr1+1
        cmp     gend+1
        beq     @done
@2:     lda     (ptr1),y
        cmp     #NL
        beq     @done
        inc     ptr1
        bne     @loop
        inc     ptr1+1
        bne     @loop           ; always
@done:  jmp     logical

; void __fastcall__ ed_rows(uint16_t off)
; Draws ed_n text rows from the screen address ed_scr down, starting with
; the line at logical offset off: one line per row, the row with index
; ed_cur scrolled by ed_hs characters (`<` in its column 0), every row
; `>` in column 39 if its line goes on beyond the window, blank rows below
; the last line. ed_scr ends after the last row drawn.
_ed_rows:
        jsr     phys
        lda     #0
        sta     row
@loop:  lda     #0
        sta     hsrow
        sta     hsrow+1
        lda     row
        cmp     _ed_cur
        bne     @draw
        lda     _ed_hs
        sta     hsrow
        lda     _ed_hs+1
        sta     hsrow+1
@draw:  jsr     line1           ; at the end of the text ptr1 stays at gend:
        lda     _ed_scr         ; the rows below come out blank
        clc
        adc     #40
        sta     _ed_scr
        bcc     :+
        inc     _ed_scr+1
:       inc     row
        lda     row
        cmp     _ed_n
        bne     @loop
        rts

; Draw the line at physical address ptr1 on the row at ed_scr, hsrow
; characters skipped; ptr1 ends after its '\n' (or at gend: last line).
line1:
        lda     g0              ; first segment if ptr1 < g1, i.e. <= g0
        cmp     ptr1
        lda     g0+1
        sbc     ptr1+1
        bcc     @second
        lda     g0
        ldx     g0+1
        bcs     @setlim         ; always
@second:
        lda     gend
        ldx     gend+1
@setlim:
        sta     lim
        stx     lim+1
        lda     #0
        sta     col
        sta     more
        lda     hsrow
        sta     cnt
        lda     hsrow+1
        sta     cnt+1

; ---- skip hsrow characters (the line may end before that)
@skip:  lda     cnt
        ora     cnt+1
        beq     @copy
        jsr     avail
        bcc     :+
        jmp     @eof
:       ldx     cnt+1           ; chunk = min(available, cnt)
        bne     @sk1
        cmp     cnt
        bcc     @sk1
        lda     cnt
@sk1:   sta     n
        ldy     #0
@skl:   lda     (ptr1),y
        cmp     #NL
        beq     @sknl
        iny
        cpy     n
        bne     @skl
        jsr     advy
        lda     cnt             ; cnt -= n
        sec
        sbc     n
        sta     cnt
        bcs     @skip
        dec     cnt+1
        jmp     @skip
@sknl:  jmp     @nl             ; the line ends left of the window

; ---- copy up to 40 - col characters, converted, to the row
@copy:  lda     #40
        sec
        sbc     col
        beq     @rest           ; window full
        sta     tmp1
        jsr     avail
        bcc     :+
        jmp     @eof
:       cmp     tmp1            ; chunk = min(available, room)
        bcc     @cp1
        lda     tmp1
@cp1:   sta     n
        lda     _ed_scr         ; ptr2 = row + col
        clc
        adc     col
        sta     ptr2
        lda     _ed_scr+1
        adc     #0
        sta     ptr2+1
        ldy     #0
@cpl:   lda     (ptr1),y        ; about 29 cycles per character
        cmp     #NL
        beq     @cpnl
        tax
        lda     _ed_tbl,x
        sta     (ptr2),y
        iny
        cpy     n
        bne     @cpl
        jsr     advy
        lda     col
        clc
        adc     n
        sta     col
        jmp     @copy
@cpnl:  tya
        clc
        adc     col
        sta     col
        jmp     @nl

; ---- window full: is there more of this line? find its end
@rest:  jsr     avail
        bcs     @eof
        sta     n
        ldy     #0
        lda     (ptr1),y
        cmp     #NL
        beq     @nl
        sta     more            ; not 0: the text holds no NUL
@rsl:   lda     (ptr1),y
        cmp     #NL
        beq     @nl
        iny
        cpy     n
        bne     @rsl
        jsr     advy
        jmp     @rest

; ---- end of the line: Y = index of its '\n' in the current chunk
@nl:    iny
        jsr     advy
@eof:                           ; ptr1 = gend: avail found the end
        ; fall through

; blank the row from col on, then the markers
finish: lda     _ed_scr
        sta     ptr2
        lda     _ed_scr+1
        sta     ptr2+1
        ldy     col
        lda     #' '
@pad:   cpy     #40
        beq     @mk
        sta     (ptr2),y
        iny
        bne     @pad
@mk:    lda     hsrow
        ora     hsrow+1
        beq     @mk2
        ldy     #0
        lda     #'<'            ; screen code = ASCII for $20-$3F
        sta     (ptr2),y
@mk2:   lda     more
        beq     @done
        ldy     #39
        lda     #'>'
        sta     (ptr2),y
@done:  rts

; A = characters from ptr1 to the end of its segment (at most 255), C clear.
; At the end of the first segment ptr1 moves on to g1; at the end of the
; text C is set.
avail:  sec
        lda     lim
        sbc     ptr1
        tax
        lda     lim+1
        sbc     ptr1+1
        bne     @big
        txa
        bne     @ok
        lda     lim             ; segment done: which one?
        cmp     gend
        bne     @next
        lda     lim+1
        cmp     gend+1
        bne     @next
        sec                     ; end of the text
        rts
@next:  lda     g1
        sta     ptr1
        lda     g1+1
        sta     ptr1+1
        lda     gend
        sta     lim
        lda     gend+1
        sta     lim+1
        jmp     avail
@big:   lda     #255
@ok:    clc
        rts

; ptr1 += Y
advy:   tya
        clc
        adc     ptr1
        sta     ptr1
        bcc     :+
        inc     ptr1+1
:       rts

; void __fastcall__ ed_put(const char *s)
; ASCII s (at most 40 characters used) on the row at ed_scr, the rest of
; the row blank; every cell EOR ed_rev (reverse video for the bars).
_ed_put:
        sta     ptr1
        stx     ptr1+1
        lda     _ed_scr
        sta     ptr2
        lda     _ed_scr+1
        sta     ptr2+1
        ldy     #0
@l:     lda     (ptr1),y
        beq     @pad
        tax
        lda     _ed_tbl,x
        eor     _ed_rev
        sta     (ptr2),y
        iny
        cpy     #40
        bne     @l
        rts
@pad:   lda     #' '
        eor     _ed_rev
@p2:    sta     (ptr2),y
        iny
        cpy     #40
        bne     @p2
        rts
