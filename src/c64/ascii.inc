; ascii.inc -- identity character map for ca65.
; ca65 -t c64 translates character constants and .byte strings to PETSCII;
; text inside the program is ASCII (src/core/ascii.h), so every assembly
; source includes this first: 'E' assembles to $45, "aZ" to $61 $5A.
.repeat 255, I
        .charmap I+1, I+1
.endrepeat
