;-----------------------------------------------------------------------------
;
;               =====================================
;               105 color image viewer for MSX   v1.0
;               =====================================
;
;  (c) 2006 Daniel Vik
;
;  This software is provided 'as-is', without any express or implied warranty.
;  In no event will the authors be held liable for any damages arising from
;  the use of this software.
;
;  Permission is granted to anyone to use this software for any purpose,
;  including commercial applications, and to alter it and redistribute it
;  freely, subject to the following restrictions:
;
;  1. The origin of this software must not be misrepresented; you must not
;     claim that you wrote the original software. If you use this software
;     in a product, an acknowledgment in the product documentation would be
;     appreciated but is not required.
;  2. Altered source versions must be plainly marked as such, and must not be
;     misrepresented as being the original software.
;  3. This notice may not be removed or altered from any source distribution.
;
;-----------------------------------------------------------------------------


; This package views 105 color interlaced screen2 images.
;
; The image should have the following format:
;   width               - 1 byte
;   height              - 1 byte
;   pattern even frame  - width * height bytes
;   colors even frame   - width * height bytes
;   pattern odd frame   - width * height bytes
;   colors odd frame    - width * height bytes
;
; The user is responsible for reserving 128 bytes of RAM
; that will be used by the image viewer. The RAM should
; be pointed to by the label 'Image105Data'
;
; The viewer code does not check that the image fits on
; the screen and if not, the result is unexpected.
;
; The viewer will use characters 1-255 in each segment
; of the screen that contains the picture. The viewer
; does never use character 0 and the image doesn't need
; to use all characters 1-255 it will not do so. 
; If so, the unused characters are located between
; N to 127 and (128 + N) to 255 where N is the number of
; unused characters. 
; So for example if the image is 16x20 characters big and
; is located in the top left corner of the screen, character
; 64-127 and 192-255 in the bottom segment of the screen
; are not used.
;

;---------------------------------------------------------
; Variables
;---------------------------------------------------------
Image105_PatchColorOdd0:    equ Image105Data + $00
Image105_PatchColorOdd1:    equ Image105Data + $08
Image105_PatchColorOdd2:    equ Image105Data + $10
Image105_PatchPatternOdd0:  equ Image105Data + $18
Image105_PatchPatternOdd1:  equ Image105Data + $20
Image105_PatchPatternOdd2:  equ Image105Data + $28
Image105_PatchColorEven0:   equ Image105Data + $30
Image105_PatchColorEven1:   equ Image105Data + $38
Image105_PatchColorEven2:   equ Image105Data + $40
Image105_PatchPatternEven0: equ Image105Data + $48
Image105_PatchPatternEven1: equ Image105Data + $50
Image105_PatchPatternEven2: equ Image105Data + $58
Image105_X:                 equ Image105Data + $60
Image105_Y:                 equ Image105Data + $61
Image105_Width:             equ Image105Data + $62
Image105_Height:            equ Image105Data + $63
Image105_EvenOdd:           equ Image105Data + $66
Image105_TempAddr:          equ Image105Data + $68
Image105_TempVal:           equ Image105Data + $6a

                
        
;---------------------------------------------------------
; Loads 105 color image
; In:
;       hl  -   si2 Image data address
;       b   -   x offset (in chars, 0-31)
;       c   -   y offset (in chars, 0-23)
;---------------------------------------------------------
Image105Load:
        ; Clear even/odd flag
        xor     a
        ld      (Image105_EvenOdd), a
        
        ; Save image position
        ld      a, b
        ld      (Image105_X), a
        ld      a, c
        ld      (Image105_Y), a
        
        ; Load header (assume image fits on screen)
        ld      a, (hl)
        ld      (Image105_Width), a
        inc     hl
        ld      a, (hl)
        ld      (Image105_Height), a
        inc     hl

        xor     a
        ld      de, Image105_PatchPatternEven0
        call    ImageLoadBlock
        ld      a, $20
        ld      de, Image105_PatchColorEven0
        call    ImageLoadBlock
        ld      a, $04
        ld      de, Image105_PatchPatternOdd0
        call    ImageLoadBlock
        ld      a, $24
        ld      de, Image105_PatchColorOdd0
        call    ImageLoadBlock
                
        ret
        
        
;---------------------------------------------------------
; Updates 105 color image. Should be called once per frame
;---------------------------------------------------------
Image105Update:
        ld      a, (Image105_EvenOdd)
        xor     $80
        ld      (Image105_EvenOdd), a
        ld      c, a
        
        ld      a, (Image105_X)
        ld      d, a
        
        ld      a, (Image105_Height)
        ld      h, a

        ld      a, (Image105_Y)
        ld      e, a
                
        ; Let b = 8 - (Y and 7) (nr of lines in first 1/3 of screen)
        and     7
        neg
        add     8
        ld      b, a
        
.Loop:  
        ; Calculate b = MIN(b, h)
        ld      a, b
        sub     h
        jr      c, .NoMin
        ld      b, h
.NoMin:
        call    Image105UpdateBlock
        
        ; e = e + b
        ld      a, e
        add     b
        ld      e, a
        
        ; h = h - b
        ld      a, h
        sub     b
        ld      h, a
        
        ld      b, 8
        
        ; If more lines to draw, do so
        jr      nz, .Loop
                        
        ret

        
;---------------------------------------------------------
; Updates patch character.
;   In:
;       a   - y offset (0, 8, 16)
;       c   - Even/Odd ($00 or $80)
;---------------------------------------------------------
Image105UpdatePatchChar:
        push    bc
        push    de

        ; Get pointer to color patch
        ld      d, 0
        ld      e, a

        ld      a, c
        cp      0
        ld      hl, Image105_PatchColorEven0
        jr      nz, .Even
        ld      hl, Image105_PatchColorOdd0
.Even:
        add     hl, de
        ; Output color patch
        ld      a, $00
        out     [$99], a
        ld      a, e
        or      $64
        nop
        out     [$99], a
        ld      bc, $0898
.LoopColor:
        outi
        jr      nz, .LoopColor

        ; Get pointer to pattern patch
        ld      bc, $10
        add     hl, bc
        
        ; Output pattern patch
        ld      a, $00
        out     [$99], a
        ld      a, e
        or      $44
        nop
        out     [$99], a
        ld      bc, $0898
.LoopPattern:
        outi
        jr      nz, .LoopPattern
        
        pop     de
        pop     bc
        ret


;---------------------------------------------------------
; Updates image on 1/3 of the screen
;   In:
;       b   - number of rows
;       c   - Even/0dd ($00 or $80)
;       d   - x offset
;       e   - y offset
;---------------------------------------------------------
Image105UpdateBlock:
        push    bc
        push    de
        push    hl
        
        ld      a, e
        and     $f8
        call    Image105UpdatePatchChar
        
        ld      a, c
        xor     $ff
        ld      c, a

        ld      l, $80
        
        ld      a, e
        rrca
        rrca
        rrca
        and     $e0
        add     d
        sub     $20
        ld      d, a
        
.LoopY:
        ld      a, d
        add     $20
        ld      d, a
        out     [$99], a
        ld      a, e
        rrca
        rrca
        rrca
        and     $1f
        or      $58
        out     [$99], a

        ld      a, (Image105_Width)
        ld      h, a
        ld      a, l
.LoopX:
        out     [$98], a
        inc     a
        and     c
        dec     h
        jr      nz, .LoopX
        ld      l, a
        inc     e
        djnz    .LoopY
        
        pop     hl
        pop     de
        pop     bc
        
        ret
       
;---------------------------------------------------------
; Loads even/odd pattern/color block
;   In:
;       a   - VRAM base offset ($00=pattern, $20=color)
;       de  - Patch data address
;       hl  - Image data address
;---------------------------------------------------------
ImageLoadBlock:
        ld      (Image105_TempAddr), de
        ld      (Image105_TempVal), a
        
        ld      a, (Image105_Height)
        ld      d, a
        
        ld      a, (Image105_Y)
        ld      e, a
        
        and     7
        neg
        add     8
        ld      b, a
        
.Loop:  
        ld      a, b
        sub     d
        jr      c, .NoMin
        ld      b, d
.NoMin: 
        ; Save Patch data
        push    bc
        push    de
        ld      a, e
        and     $f8
        push    hl
        ; Get pointer to color patch
        ld      h, 0
        ld      l, a
        ld      de, (Image105_TempAddr)
        add     hl, de
        ex      de, hl
        ld      bc, 8
        pop     hl
        ldir
        pop     de
        pop     bc
        
        ; Load image data
        push    bc
        push    de
        
        ld      a, 8
        out     [$99], a
        ld      a, e
        and     $f8
        ld      e, a
        ld      a, (Image105_TempVal)
        or      e
        or      $40
        out     [$99], a
        
        ld      d, b
        ld      a, (Image105_Width)
        rla
        rla
        rla
        ld      e, a
        sub     8
        ld      b, a
        ld      c, $98
.CopyLoop:
        outi
        jr      nz, .CopyLoop
        ld      b, e
        dec     d
        jr      nz, .CopyLoop
              
        pop     de
        pop     bc
        
        ; Update counters
        ld      a, e
        add     b
        ld      e, a
        ld      a, d
        sub     b
        ld      d, a
        ld      b, 8
        jr      nz, .Loop
        
        ret
