; gfx_c64.s -- Bresenham line into the hi-res bitmap at $E000 (VIC bank 3).
;
; Called by gfx_line (gfx_c64.c) after clipping, so the endpoints are always
; inside 0..319 x 0..199 and every pixel lies in $E000-$FF3F:
;   pixel (x,y) = byte $E000 + (y>>3)*320 + (x>>3)*8 + (y&7), bit 7-(x&7).
; The pointer ptr1 holds the address of the current 8x8 cell row base
; ($E000 + (y>>3)*320 + (x>>3)*8), Y holds y&7 and `mask` the pixel bit, so
; plotting is LDA (ptr1),Y / ORA / STA and every step is an increment:
;   x+1: mask >>= 1; when the bit falls out, mask = $80 and ptr1 += 8
;   y+1: Y += 1; at 8, Y = 0 and ptr1 += 320    (y-1 the same way down)
; The endpoints are ordered so that x always grows. With dx = x1-x0 and
; dy = |y1-y0|, the loop runs along the longer axis (dx >= dy: x-major)
; for max(dx,dy)+1 pixels, err starts at major/2, minus minor per pixel,
; and when it goes negative it gets +major and the minor axis steps.
; tools/check_bitmap.py has a Python model of exactly this loop.
;
; Reading the bitmap needs the KERNAL ROM out: the whole line runs with
; interrupts off and $01 = %xx101 (RAM at $A000-$BFFF and $E000-$FFFF, I/O
; in), restored afterwards. A 320-pixel line keeps interrupts off for about
; 20 ms (one jiffy IRQ is delayed, not lost). An NMI (RESTORE) in that
; window vectors through RAM $FFFA, which gfx_open points at gfx_nmi_rti.

        .export _gfx_draw, _gfx_nmi_rti
        .export _gfx_x0, _gfx_y0, _gfx_x1, _gfx_y1
        .importzp ptr1, ptr2, ptr3, tmp1, tmp2, tmp3

err     = ptr2          ; 16-bit Bresenham error (x-major); low byte only (y-major)
cnt     = ptr3          ; pixels left after the current one
mask    = tmp1          ; pixel bit in the current byte
dy      = tmp2          ; |y1 - y0|, 0..199
ydir    = tmp3          ; bit 7 set: y decreases

.bss
; keep x0,y0 and x1,y1 adjacent: the swap below indexes them as 3-byte records
_gfx_x0: .res 2
_gfx_y0: .res 1
_gfx_x1: .res 2
_gfx_y1: .res 1
dx:      .res 2         ; x1 - x0, 0..319
save01:  .res 1

.rodata
bits:   .byte $80, $40, $20, $10, $08, $04, $02, $01

.code
_gfx_nmi_rti:
        rti

_gfx_draw:
; order the endpoints so that x0 <= x1
        lda     _gfx_x1
        cmp     _gfx_x0
        lda     _gfx_x1+1
        sbc     _gfx_x0+1
        bcs     ordered
        ldx     #2
swap:   lda     _gfx_x0,x
        ldy     _gfx_x1,x
        sta     _gfx_x1,x
        tya
        sta     _gfx_x0,x
        dex
        bpl     swap
ordered:
; dx = x1 - x0
        lda     _gfx_x1
        sec
        sbc     _gfx_x0
        sta     dx
        lda     _gfx_x1+1
        sbc     _gfx_x0+1
        sta     dx+1
; dy = |y1 - y0|, ydir
        ldx     #0
        lda     _gfx_y1
        sec
        sbc     _gfx_y0
        bcs     :+
        eor     #$FF
        adc     #1              ; C = 0: A = -A
        dex                     ; ydir = $FF: up
:       sta     dy
        stx     ydir
; ptr1 = $E000 + row*320 + (x0 & $FFF8), row = y0>>3; row*320 = row*256 + row*64
        lda     _gfx_y0
        lsr
        lsr
        lsr
        sta     ptr1+1          ; row (temporarily)
        ldx     #0
        stx     ptr1
        lsr
        ror     ptr1
        lsr
        ror     ptr1            ; A = row>>2, ptr1 = (row&3)<<6: row*64; C = 0
        adc     ptr1+1
        adc     #$E0            ; high byte: $E0 + row + row*64/256 (no carry, max $FE)
        sta     ptr1+1
        lda     _gfx_x0
        and     #$F8
        adc     ptr1            ; C = 0 from above
        sta     ptr1
        lda     _gfx_x0+1
        adc     ptr1+1
        sta     ptr1+1
        lda     _gfx_x0
        and     #7
        tax
        lda     bits,x
        sta     mask
        lda     _gfx_y0
        and     #7
        tay
; KERNAL out for the whole line
        php
        sei
        lda     $01
        sta     save01
        and     #$F8
        ora     #$05
        sta     $01
; choose the major axis
        lda     dx+1
        bne     xmajor
        lda     dx
        cmp     dy
        bcc     ymajor

; ---- x-major: dx >= dy, dx+1 pixels
xmajor: lda     dx+1
        sta     cnt+1
        lsr
        sta     err+1
        lda     dx
        sta     cnt
        ror
        sta     err             ; err = dx/2
xloop:  lda     (ptr1),y
        ora     mask
        sta     (ptr1),y
        lda     cnt             ; cnt-- or done
        bne     :+
        lda     cnt+1
        beq     done
        dec     cnt+1
:       dec     cnt
        lsr     mask            ; x+1
        bcc     :+
        ror     mask            ; mask = $80, C = 0
        lda     ptr1
        adc     #8
        sta     ptr1
        bcc     :+
        inc     ptr1+1
:       lda     err             ; err -= dy
        sec
        sbc     dy
        sta     err
        bcs     xloop           ; no borrow: err >= 0 still
        lda     err+1
        sbc     #0
        sta     err+1
        bpl     xloop
        lda     err             ; err < 0: err += dx, y step
        clc
        adc     dx
        sta     err
        lda     err+1
        adc     dx+1
        sta     err+1
        jsr     ystep
        jmp     xloop

; ---- y-major: dy > dx (so dx < 199 fits a byte), dy+1 pixels.
; err stays in 0..dy-1 < 256, so 8 bits with the borrow as the sign suffice.
ymajor: lda     dy
        sta     cnt
        lsr
        sta     err
yloop:  lda     (ptr1),y
        ora     mask
        sta     (ptr1),y
        lda     cnt
        beq     done
        dec     cnt
        jsr     ystep
        lda     err             ; err -= dx
        sec
        sbc     dx
        bcs     ynox            ; no borrow: no x step
        adc     dy              ; C = 0: err += dy (back in 0..dy-1)
        sta     err
        lsr     mask            ; x+1
        bcc     yloop
        ror     mask            ; mask = $80, C = 0
        lda     ptr1
        adc     #8
        sta     ptr1
        bcc     yloop
        inc     ptr1+1
        bne     yloop           ; always (ptr1+1 is $E0-$FF)
ynox:   sta     err
        bcs     yloop           ; always

done:   lda     save01
        sta     $01
        plp
        rts

; one pixel row down (ydir clear) or up; changes A and Y
ystep:  bit     ydir
        bmi     up
        iny
        cpy     #8
        bne     yret
        ldy     #0
        lda     ptr1
        clc
        adc     #<320
        sta     ptr1
        lda     ptr1+1
        adc     #>320
        sta     ptr1+1
yret:   rts
up:     dey
        bpl     yret
        ldy     #7
        lda     ptr1
        sec
        sbc     #<320
        sta     ptr1
        lda     ptr1+1
        sbc     #>320
        sta     ptr1+1
        rts
