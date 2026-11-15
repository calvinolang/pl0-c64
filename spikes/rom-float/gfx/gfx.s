; gfx.s -- SPIKE: OR a pixel mask into bitmap RAM under the KERNAL ROM.
; Reading $E000-$FFFF returns KERNAL ROM unless HIRAM is cleared, so the
; read-modify-write runs with $01 = $35 (BASIC+KERNAL out, I/O in) and
; interrupts off (the IRQ vector at $FFFE would come from RAM otherwise).
; A real line routine banks once per line, not per pixel.
        .export _bm_or, _bm_mask
        .importzp ptr1

.bss
_bm_mask: .res 1

.code
_bm_or:                         ; A/X = address
        sta     ptr1
        stx     ptr1+1
        ldy     #0
        php
        sei
        lda     $01
        pha
        lda     #$35
        sta     $01
        lda     (ptr1),y
        ora     _bm_mask
        sta     (ptr1),y
        pla
        sta     $01
        plp
        rts
