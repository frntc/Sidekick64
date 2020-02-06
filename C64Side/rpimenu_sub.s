.proc   _detectC128
.export _detectC128
_detectC128:

; from: http://cbm-hackers.2304266.n4.nabble.com/Re-Detect-a-C128-from-C64-mode-td4656728.html
; Checks for the output of the additional C128 VIC register
; at $d030. In a 64 it reads $ff. In a C128 in 64 mode
; it outputs other values as it has the two bits controlling
; "test" and "clock speed" modes
;
; INPUTS: none
; RETURNS: C128 flag in A
; REGISTERS AFFECTED: A

c128_check:
        lda $d030
        cmp #$ff
        beq c64
        bne done_c128
c128:
        lda #$ff
        sta $d030
done_c128:
        lda #$01
        rts

c64:
        lda #$fc
        sta $d030
        eor $d030
        beq c128
done_c64:
        lda #$00
        rts

.endproc



.proc   _detectVIC
.export _detectVIC
_detectVIC:

; from: https://codebase64.org/doku.php?id=base:detect_vic_type
; ---------------------------------------------------------------------------
; VIC-Check 2 written by Crossbow/Crest (shamelessly disassembled by Groepaz)
; ---------------------------------------------------------------------------

                 JSR     $FF81 ; clear screen

                 SEI           ; disable irq

                 ; init VIC registers
                 LDX     #$2F

loc_813:                    
                 LDA     vicregs,X
                 STA     $D000,X
                 DEX
                 BNE     loc_813

                 ; clear sprite data at $2800
                 TXA

loc_81D:                    
                 STA     $2800,X
                 INX
                 BNE     loc_81D

                 LDA     #$10
                 STA     $2800

                 ; put a black reversed space at start of charline 22
                 LDA     #$A0
                 STA     $770
                 STA     $7FB      ; sprite pointer
                 STA     $DB70

mainloop:                    

                 ; wait for rasterline $e4       
                 LDA     #$E4
loc_835:                                
                 CMP     $D012
                 BNE     loc_835

                 ; waste some cycles
                 LDX     #3
loc_83C:
                 DEX
                 BNE     loc_83C

                 LDX     #$1C
                 STX     $D011     ; $d011=$1c

                 LDY     #$5B
                 DEX
                 STX     $D011     ; $d011=$1b

                 ; 4 cycles more for ntsc
                 LDA     $2A6
                 BNE     loc_851

                 NOP
                 NOP

loc_851:
                 STY     $D011     ; $d011=$5b (enables ECM)
                 STX     $D011     ; $d011=$1b

                 ;LDX     $D01F     ; sprite/background collision (will be either 0 or 8)

                LDA #$14
                STA $d020
                LDA #$6    
                STA $d021
                JSR     $FF81 ; clear screen
                CLI

                 LDA     $D01F     ; sprite/background collision (will be either 0 or 8)
                 rts
.endproc

                 ; display result
;                 LDY     #0
;loc_85C:
;                 LDA     victype,x
;                 STA     $400,Y
;                 LDA     #1
;                 STA     $D800,Y
;                 INX
;                 INY
;                 CPY     #8
;                 BNE     loc_85C

;                 JMP     mainloop

; ---------------------------------------------------------------------------
;victype:
 ;                !scr "old vic "
  ;               !scr "new vic "

vicregs:
                 .BYTE   0,0,0,0, 0,0,$18,$E4, 0,0,0,0, 0,0,0,0 ; sprite x pos
                 .BYTE   0 ; sprite x msb
                 .BYTE $9B ; $d011
                 .BYTE  $E ; $d012
                 .BYTE $3E ; $d013 latch x
                 .BYTE $6C ; $d014 latch y
                 .BYTE   8 ; $d015 Sprite display Enable
                 .BYTE $C8 ; $d016
                 .BYTE   0 ; $d017 Sprites Expand 2x Vertical (Y)
                 .BYTE $15 ; $d018 Memory Control Register
                 .BYTE $79 ; $d019 Interrupt Request Register (IRR)
                 .BYTE $F0 ; $d01a Interrupt Mask Register (IMR)
                 .BYTE 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0