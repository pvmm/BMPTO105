;---------------------------------------------------------
; Simple demonstration of 105 color image viewer
;---------------------------------------------------------


        fname   "img105.bin"
		db	$fe
		dw	Init,End,Begin
        org     $9000
        
        FORCLR:     equ $f3e9
        INIGRP:     equ $72
        FILVRM:     equ $56
        
Init:

;---------------------------------------------------------
; Entry point
;---------------------------------------------------------
Begin:
        ld      a,1
        ld      hl,FORCLR
	    ld      (hl),a
	    inc     hl
	    ld      (hl),a
	    inc     hl
	    ld      (hl),a
        
        call    INIGRP
        
        xor     a
        ld      bc,$4000
        ld      hl,$0000
        call    FILVRM

        ld      hl,ImageData
        ld      bc,$0800
        call    Image105Load
.Loop:
        halt
        di
        call    Image105Update
        
        in      a,[$aa]
        and     240
        or      8
        out     [$aa],a
        nop
        nop
        in      a,[$a9]
        and     1
        ei
        jp      nz,.Loop
        
        ld      a,15
        ld      hl,FORCLR
	    ld      (hl),a
        ld      a,4
	    inc     hl
	    ld      (hl),a
	    inc     hl
	    ld      (hl),a
        
        call    INIGRP

        ret

;---------------------------------------------------------
; Include Image105 viewer
;---------------------------------------------------------
INCLUDE "I105CORE.ASM"
         
                
;---------------------------------------------------------
; Include image
;---------------------------------------------------------
ImageData:
INCBIN "image.si2"


End:    equ $

;---------------------------------------------------------
; Variables
;---------------------------------------------------------
Image105Data:
rb  $70

