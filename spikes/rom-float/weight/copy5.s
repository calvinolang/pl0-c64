; copy5.s -- SPIKE: 5-byte copy *_s -> *_d for vm2.c built with -DASMCOPY
        .export _copy5
        .import _s, _d
        .importzp ptr1, ptr2
_copy5: lda     _s
        sta     ptr1
        lda     _s+1
        sta     ptr1+1
        lda     _d
        sta     ptr2
        lda     _d+1
        sta     ptr2+1
        ldy     #4
@l:     lda     (ptr1),y
        sta     (ptr2),y
        dey
        bpl     @l
        rts
