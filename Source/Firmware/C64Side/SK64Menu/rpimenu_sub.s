;
;  _________.__    .___      __   .__        __           _____                         
; /   _____/|__| __| _/____ |  | _|__| ____ |  | __      /     \   ____   ____  __ __   
; \_____  \ |  |/ __ |/ __ \|  |/ /  |/ ___\|  |/ /     /  \ /  \_/ __ \ /    \|  |  \  
; /        \|  / /_/ \  ___/|    <|  \  \___|    <     /    Y    \  ___/|   |  \  |  /  
;/_______  /|__\____ |\___  >__|_ \__|\___  >__|_ \    \____|__  /\___  >___|  /____/   
;        \/         \/    \/     \/       \/     \/            \/     \/     \/         
;
;
; Sidekick64 - A framework for interfacing the C64 and a Raspberry Pi Zero 2 or 3A+/3B+
; Copyright (c) 2019-2022 Carsten Dachsbacher <frenetic@dachsbacher.de>
;
; Logo created with http://patorjk.com/software/taag/
; 
; This program is free software: you can redistribute it and/or modify
; it under the terms of the GNU General Public License as published by
; the Free Software Foundation, either version 3 of the License, or
; (at your option) any later version.
;
; This program is distributed in the hope that it will be useful,
; but WITHOUT ANY WARRANTY; without even the implied warranty of
; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
; GNU General Public License for more details.
; 
; You should have received a copy of the GNU General Public License
; along with this program.  If not, see <http://www.gnu.org/licenses/>.

.export _myInitVideo
.export _spritesLogo
.export _setupSpriteLogo

.import _cbm_k_scnkey

RELOC_BASE = $7000

lastVDCUpdateLine = $08f9
enableVDC = $08f8
switchVDCMode = $08f7
currentVDCMode = $08f6
delayKey = $08ff
lastKey = $08fe
lastKeyWasRepetition = $08fd
showLogo = $08fc
keyPressed = $08fb

; 2x bytes
.macro burn_3x_Cycles val
	.repeat val
	bit $01
	.endrep
.endmacro

; 5 bytes
.macro burn_5xPlus1_Cycles val
	ldx #val	; 2c				
	dex	        ; x * 2c			
	bne *-1         ; (x-1) * 3c + 1 * 2c		
.endmacro

.macro sendSidekickCmd cmd, data
        lda #0
        sta receivedReadySignal

        lda $9ffe	
        lda $9d00+data
	lda $9e00+cmd
.endmacro

; send command 'cmd' with data in X to Sidekick
.macro sendSidekickCmdX cmd
        lda #0
        sta receivedReadySignal

        lda $9ffe	
        lda $9d00,x
	lda $9e00+cmd
.endmacro

; send command in X with data in Y to Sidekick
.macro sendSidekickCmdXY
        lda #0
        sta receivedReadySignal

        lda $9ffe	
        lda $9d00,y
	lda $9e00,x
.endmacro

.macro wait4Sidekick
.local sk64busy
        lda #0
    sk64busy:
        cmp receivedReadySignal
        beq sk64busy
        sta receivedReadySignal
.endmacro

.macro vdcWrite reg, d
.local wait
        lda #reg
        sta $d600
        lda #d
        wait:
        bit $d600
        bpl wait
        sta $d601
.endmacro

.proc   _initCharset
.export _initCharset
_initCharset:
        ldx #2
        ldy #0

        sty lastVDCUpdateLine
        sty enableVDC
        sty switchVDCMode
        sty currentVDCMode

        jsr RELOC_BASE+$2e0 ;//_callSidekickCommand

        rts
.endproc

.proc   _loop
.export _loop
_loop:

	jmp RELOC_BASE+$07 

	;// here is address RELOC_BASE+$03
	jsr $0000
	rts

mainloop:

	ldx #0 
        stx keyPressed

	lda #1
	sta $28b
	sta $28c

scanJoystick:

        ;// joystick goes here
        lda #%00000001
        bit $dc00
        bne noJoyUp
          ldx #145
        noJoyUp:

        lda #%00000010
        bit $dc00
        bne noJoyDn
          ldx #17
        noJoyDn:

        lda #%00000100
        bit $dc00
        bne noJoyLeft
          ldx #157
        noJoyLeft:
        
        lda #%00001000
        bit $dc00
        bne noJoyRight
          ldx #29
        noJoyRight:

        lda #%00010000
        bit $dc00
        bne noJoyBtn
          ldx #13
        noJoyBtn:

        txa
        sta keyPressed 

        ;// key could be != 0 when joystick is used
	cmp #0
        bne dontReadKey

        lda $c6
        cmp #0
        beq dontReadKey ;// $c6 == 0 -> ok

        lda $0277
        cmp lastKey
        beq bla
        lda #0
        sta delayKey
        bla:

        jsr $e5b4		;// get it!
        sta keyPressed 

dontReadKey:

        ; if ( delayKey == 0 ) goto continueWithKey
        ldy delayKey
        beq continueWithKey
  
        ; delayKey > 0 => if ( --delayKey == 0 ) goto continueWithKey
        dec delayKey
        beq continueWithKey

        ; while we're waiting: if ( key == 0 ) lastKey := 0
        ldx keyPressed
        bne noLastCharOverwrite
        stx lastKey
        stx $c6
        stx $277
        noLastCharOverwrite:

        ldx #0
        jmp noKeyHandling - _loop + RELOC_BASE

continueWithKey:

	ldx keyPressed 	;// key

        ; no key? then goto noKeyHandling
        cpx #0
        beq noKeyHandling

        ; we got a key press (key != 0)
	
        ldx keyPressed 	;// key

        ldy #0
        sty $277
        sty $c6
        ldy #1

        cpx lastKey
        beq keyRepeatOrNoKey
gotNewKey:
        ldy #15
        stx lastKey

keyRepeatOrNoKey:
        sty delayKey
        sty lastKeyWasRepetition

        cpx #0
        beq notstore
        stx lastKey
        notstore:
        
noKeyHandling:      

        lda keyPressed
        cmp #0
        bne keyWasNotZero_or_lastKeyWasNoRepetition
        lda lastKeyWasRepetition
        cmp #0
        beq keyWasNotZero_or_lastKeyWasNoRepetition
        lda #0
        sta lastKey
        sta lastKeyWasRepetition
keyWasNotZero_or_lastKeyWasNoRepetition:

        ; send key
        lda #0
        sta receivedReadySignal
	lda $9ffe       ;// signal that we're sending a command + data
	lda $9d00,x	;// implicitly send data
        lda $9e01       ;// implicitly send command "send key"


        ;ldx #2
        ;stx $d020

	;// return value in A

	cmp #128 
	bne noPostCommand 

                ; update color RAM to show hidden message
		lda $427 	
		beq no_print_wait 	

		ldx keyPressed 	
		cpx #29 	
		beq print_wait 	
		cpx #83 	
		beq print_wait 	
		cpx #115 	
		bne no_print_wait 

		print_wait:
                        lda #$0a
                        ldx #$20
                        loopPrintWait:
                        sta $d850,x
                        dex
                bne loopPrintWait

		no_print_wait:

noPostCommand:

        jsr _cbm_k_scnkey

        ; only for debugging purposes -> the c64 could do something else instead
        ;wait4Sidekick
        ;lda #6
        ;sta $d020

        ;
        ; the following part is C128-specific and calls the VIC->VDC screen copy code, plus optionally switches VDC modes
        ;

        ;
        ; note: the actual transfer of VDC data has been moved into the speedcode call (jsr $a000)
        ;       here we only take care of switching VDC modes and $d030
        ;

        ldx enableVDC
        cpx #2
        beq VDConly 

        ; we will use the VDC
        lda #1
        sta $d01a
        lsr             ; A=0
        sta $d030

        cpx #0
        ; VIC + VDC together
        bne continueWithVDCTransfer

        jmp noVDCOutput - _loop + RELOC_BASE

VDConly:
        ldx #0
        stx $d011
        stx $d01a
        stx $d020
        inx ; 2 MHz mode, VIC will be disabled
        stx $d030

 
continueWithVDCTransfer:
        lda switchVDCMode
        beq noModeSwitching
        
        sta currentVDCMode
        
        ldy #26
        cmp #2
        beq switch80Cols
        ldy #12

switch80Cols:
        ; switch to 40 or 80 column mode with 40/48 characters per row
        ldx #6
writeReg4080:
        ;lda switch4080table - _loop + RELOC_BASE, y
        lda $5600, y
        sta $d600
        lda $5600+1, y
        wait_b:
        bit $d600
        bpl wait_b
        sta $d601
        dey
        dey
        dex 
        bpl writeReg4080

        jmp noVDCOutput - _loop + RELOC_BASE

noModeSwitching:
        ldx #0
        lda currentVDCMode
        cmp #2
        bne noSecondUpdate
        ldx enableVDC
        dex                     ; 0 = VIC+VDC, 1 = VDC only
noSecondUpdate:
        txa
        pha

doAnotherUpdate:
        ldx lastVDCUpdateLine
        inx
        inx
        inx
        inx
        inx
        cpx #25
        bcc noWrapVDCLine
        ldx #0
        noWrapVDCLine:
        stx lastVDCUpdateLine

        ;jsr $5000

        pla
        tax
        dex
        bpl doAnotherUpdate

noVDCOutput:
        lda #0
        sta switchVDCMode


        ; when the VDC is enabled continue, for VIC-only output wait to specific raster lone
        lda enableVDC
        cmp #0
        bne VDCOutput_nowait

        ;lda #3
        ;sta $d020

        w0: LDA $D012
        w1: CMP $D012
        BEQ w1
        BMI w0
                l1a:
                lda $d012
                cmp #60
                bcc l1a
VDCOutput_nowait:

 	; 
        wait4Sidekick

        ldx enableVDC
        cpx #2
        bne activeVIC
        lda #0
        sta $d030
        activeVIC:

        sendSidekickCmd 10, 1   ; // enable kernal replacement
	lda $e000                ;// access kernal (Sidekick64 will notice if wire connected)
        sendSidekickCmd 10, 0   ; // disable kernal replacement

        ; send command #14 to SK64 (reset SID detection)
        sendSidekickCmd 14, 0
	lda #$00
	sta $d4ff       ;// some access to the SID address range

        ldx enableVDC
        cpx #2
        bne activeVIC2
        lda #0
        sta $d030
        activeVIC2:

        ;lda #5
        ;sta $d020

        ldx enableVDC
        cpx #2
        beq VDConly2 

        lda $d011
        asl
        lda $d012
        lsr
        ;ldx #60
        tax
        sendSidekickCmdX 4   ; // current raster line for timing the NMI

        VDConly2:

        ; call speed code for screen and sprite update
	jsr $a000		

        ;lda #0
        ;sta $d020

 outofhere:
	jmp RELOC_BASE

.endproc

.proc   _singleFrameUpdate
.export _singleFrameUpdate
_singleFrameUpdate:
        ; send key
        lda #0
        sta receivedReadySignal
	lda $9ffe       ;// signal that we're sending a command + data
	lda $9d00,x	;// implicitly send data
        lda $9e01       ;// implicitly send command "send key"

        wait4Sidekick
	jsr $a000	
        rts	
.endproc


.proc   _callSidekickCommand
.export _callSidekickCommand
_callSidekickCommand:
        sendSidekickCmdXY
        wait4Sidekick
	jsr $a000
	rts
.endproc

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
                 lda    #0
                 sta    $d020
                 sta    $d021

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

;                LDA #$14
                ;STA $d020
                ;LDA #$6    
                ;STA $d021
                ;JSR     $FF81 ; clear screen
                ;jsr myInitVideo

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











;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
_myInitVideo:
; initialise screen and keyboard

LDA #$03
STA $9A
LDA #$00
STA $99
LDX #$2F
lab1: LDA TVIC,X
STA $CFFF,X
DEX
BNE lab1

LDA #$00
STA $0291
STA $CF
LDA #$48   ; low  EB48
STA $028F
LDA #$EB   ; high EB48
STA $0290
LDA #$0A
STA $0289
STA $028C
LDA #$0E
STA $0286
LDA #$04
STA $028B
LDA #$0C
STA $CD
STA $CC

LDA $0288

ORA #$80
TAY
LDA #$00
TAX
bla:
STY $D9,X
CLC
ADC #$28
BCC lab2
INY
lab2: INX
CPX #$1A
BNE bla
LDA #$FF
STA $D9,X
LDX #$18
blub:

; clear one screen line
LDY #$27
JSR routineE9F0
JSR routineEA24
lab3: 
LDA $0286
STA ($F3),Y
LDA #$20
STA ($D1),Y
DEY
BPL lab3
;;;;;

DEX
BPL blub

LDY #$00

STY $D3
STY $D6

; set address of curent screen line
LDX $D6
LDA $D3
bla2:
LDY $D9,X
BMI blub2
CLC
ADC #$28
STA $D3
DEX
BPL bla2
blub2:
JSR routineE9F0
LDA #$27
INX
bla3:
LDY $D9,X
BMI lab4
CLC
ADC #$28
INX
BPL bla3
lab4: STA $D5
JMP routineEA24

CPX $C9

BEQ bla4
lab5: LDA $D9,X
BMI lab6
DEX
BNE lab5
lab6: JMP routineE9F0

bla4:

lab7: LDA $D012
BNE lab7
LDA $D019
AND #$01
STA $02A6
JMP $FDDD

; fetch screen addresses
routineE9F0:
LDA tabECF0,X
STA $D1
LDA $D9,X
AND #$03
ORA $0288
STA $D2
RTS

; low bytes of screen line addresses
tabECF0:
.byte $00,$28,$50,$78,$A0
.byte $C8,$F0,$18,$40,$68
.byte $90,$B8,$E0,$08,$30
.byte $58,$80,$A8,$D0,$F8
.byte $20,$48,$70,$98,$C0

; set colour memory adress parallel to screen
routineEA24:
LDA $D1
STA $F3
LDA $D2
AND #$03
ORA #$D8
STA $F4
RTS


TVIC:
.byte   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 ;SPRITES (0-16)
; original
;.byte   $1B,0,0,0,0,$08,0,$14,0,0,0,0,0,0,0 ;DATA (17-31)
; modified
 .byte   0,0,0,0,0,$08,0,$14,0,0,0,0,0,0,0 ;DATA (17-31)

; original
;.byte   14,6,1,2,3,4,0,1,2,3,4,5,6,7 ;32-46
;modified
.byte   0,0,1,2,3,4,0,1,2,3,4,5,6,7 ;32-46

isC64NTSC			= RELOC_BASE+$02f0
c64Model			= RELOC_BASE+$02f3
_curScreen			= RELOC_BASE+$02f1
skipFrameCounter	        = RELOC_BASE+$02f2
receivedReadySignal     	= $08fa
curIdx     = $a4
nextRaster = $a5
prevBorder = $a6
nextBorder = $c9
nextScroll = $ca

curGradient 		= RELOC_BASE+$02f8

LUTAddr = RELOC_BASE+$0e00

.proc   _myNMI
.export _myNMI

myNMI:
        pha                                
        txa                                
        pha
        tya                                
        pha                                

        ;lda $dd0d
        ;and #%00000001
        ;beq exitNMI   

        lda #1
        ;sta $d020                         
        sta receivedReadySignal

        pla                                
        tay
        pla
        tax
        pla
        rti 
.endproc

.proc   _setupNMI
.export _setupNMI
_setupNMI:
        sei

        ldx $dd0d
        lda #%01111111
        sta $dd0d 

        ; NMI
        lda #<(RELOC_BASE+$600) 
        sta $0318
        lda #>(RELOC_BASE+$600)
        sta $0319
        rts
.endproc

.proc _minimalReset
.export _minimalReset

        ; attention: the execution of this routine is killed after a few thousand cycles (menu timeout in SK64 menu)
        sei
        
        noendofscreen:
          lda #254
          cmp $d012
          bne noendofscreen

	lda #0
	sta $d020
	sta $d021
        sta $d015
        lda $f0
        sta $d01a

        bla:
        lda $d012
        jmp bla

        rts
.endproc

.proc   _setupIRQ
.export _setupIRQ
_setupIRQ:
        ldx #0
        cpyloop:
        lda rasterLine,x
        sta LUTAddr,x
        lda rasterLine+$100,x
        sta LUTAddr+$100,x
        dex
        bne cpyloop

        lda #0
        sta isC64NTSC
        sta _curScreen
        sta skipFrameCounter
        sta curIdx
        sta nextRaster
        sta prevBorder
        sta nextBorder
        sta nextScroll
        sta curGradient

        ;    // 312 rasterlines -> 63 cycles per line PAL        => 312 * 63 = 19656 Cycles / VSYNC  => #>76  %00
        ;    // 262 rasterlines -> 64 cycles per line NTSC V1    => 262 * 64 = 16768 Cycles / VSYNC  => #>65  %01
        ;    // 263 rasterlines -> 65 cycles per line NTSC V2    => 263 * 65 = 17095 Cycles / VSYNC  => #>66  %10
        ;    // 312 rasterlines -> 65 cycles per line PAL DREAN  => 312 * 65 = 20280 Cycles / VSYNC  => #>79  %11

        jsr _detectC64Model
        sta c64Model
        cmp #0
        beq c64IsPal

        ; this machine has >63 cycles per line (NTSC or PAL-N)
        lda #1
        sta isC64NTSC

        ; if it's a DREAN PAL-N we have 312 rasterlines and keep the PAL rasterline tables
        lda c64Model
        cmp #3
        beq c64IsPal

copyNTSCTables:
        ; copy NTSC tables into right place!
        ldx #0
        cpyloopNTSC:
        lda rasterLineNTSC,x
        sta LUTAddr,x
        lda rasterLineNTSC+$100,x
        sta LUTAddr+$100,x
        dex
        bne cpyloopNTSC

c64IsPal:
        sei                                

        lda $0314
        sta RELOC_BASE + $0ef0
        lda $0315
        sta RELOC_BASE + $0ef1

        lda #<(RELOC_BASE+$300)            
        sta $0314                          
        lda #>(RELOC_BASE+$300)
        sta $0315

        lda $d01a
        ora #%00000001                     ; enable raster-IRQs
        sta $d01a

        lda #44+6*8-7                           
        sta $d012                      

        lda $d011                          
        and #%01111111                     
        sta $d011

        lda #%0111111                      ; disable timer IRQs
        sta $dc0d
        lda $dc0d                          

        lda #%0111111                      
        sta $dd0d
        lda $dd0d                          

        lda #%00000001                     
        sta $d019 

        cli
        rts
.endproc

; this is some sort of uber-IRQ handler, next time I'll be splitting something like this...
; doing it this way didn't make it easier to let the multiplexer etc run stable on both PAL and NTSC
.proc   _myIRQ
.export _myIRQ
myIRQ:
	lda $d019
	bmi rasterIRQ2                    
	lda $dc0d                         

	pla                                
	tay
	pla                                
	tax
	pla    
	rti 

rasterIRQ2:
	ldx curIdx

        ; where are we, what do we have to do?
        cpx #6-1        ; 2 cycles
        bcc doRaster    ; 2 cycles
        cpx #14-1
        bcc doSprites

doRaster:

	inx

	ldy nextBorder
	sty $d020

	lda nextScroll
	sta $d011

	lda nextRaster
	sta $d012

	lda #$ff
	sta $d019                          

        ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
        ; PAL: 27 cycles delay, NTSC 36 cycles delay
	burn_3x_Cycles 6
        nop

        lda isC64NTSC                   ; 4c (would be 3c if zero page)
        beq noAdditionalNTSCDelay1      ; 3c
        burn_3x_Cycles 1
        nop
        nop
        nop        

noAdditionalNTSCDelay1:        
	lda prevBorder

	cpx #(7+7+1)
	bcc oki ; branch if less than
	ldx #0
	oki:
	stx curIdx

	sta $d020

	lda yscroll+LUTAddr-rasterLine, x
	sta nextScroll

	lda gradient-2+LUTAddr-rasterLine, x
	sta prevBorder

	lda gradient-1+LUTAddr-rasterLine, x
	sta nextBorder

	lda rasterLine+LUTAddr-rasterLine, x
	clc
	adc skipFrameCounter

	sta nextRaster

	; 26 cycles to waste/use
        dec $e000,x
        dec $e000,x
        dec $e000,x
        nop
        nop

	sty $d020

        ; if curIdx == 1 we are in the lower border (last rasterbar before retrace)
        cpx #2
        bne noLogoSpritePlacement

        jsr placeLogoSprites - myIRQ + RELOC_BASE + $300

noLogoSpritePlacement:

rasterIRQExit2:
	pla                                ;Y vom Stack
	tay
	pla                                ;X vom Stack
	tax
	pla    
	rti 

doSprites:
        burn_3x_Cycles 1
        nop

        txa ; 2
        pha ; 3

        ; table that returns (x-6)*8+192-64
        lda RELOC_BASE+$0820+1-1, x ; +1 because "inx" moved to below
        clc
        
        tay

cont:
        ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
        ; PAL: 7 cycles delay, NTSC 12 cycles delay
        ldx isC64NTSC ; 4c (would be 3c is zero page)
        beq noAdditionalNTSCDelay
        burn_3x_Cycles 1
        nop        
noAdditionalNTSCDelay:        

	adc #1
        tax
	adc #1
	sty $07f8
	stx $07f9
	sta $07fa
	adc #1
	sta $07fb
	adc #1
	sta $07fc
	adc #1
	sta $07fd
	adc #1
	sta $07fe
	adc #1
	sta $07ff
        nop
        nop

        ; sprite y-position for the NEXT ROW of sprites!
        pla
        tax        
	ldy RELOC_BASE+$0800-2+1+1-1,x ; +1 because "inx" moved to below
	sty $d001
	sty $d003
	sty $d005
	sty $d007
	sty $d009
	sty $d00b
	sty $d00d
	sty $d00f

        ; this part sets the x-coordinates of the large multiplex-sprite-animation
        cpx #5
        bne dontDoSprites
        lda #88+24*0
        sta $d000
        lda #88+24*1
        sta $d002
        lda #88+24*2
        sta $d004
        lda #88+24*3
        sta $d006
        lda #88+24*4
        sta $d008
        lda #88+24*5
        sta $d00a
        lda #88+24*6
        sta $d00c
        lda #0 ; 89+24*7 & 255
        sta $d00e

        lda #128
        sta $d010

        lda #%11111111
        sta $d015

	;lda #255
	;sta $d01c		; multicolor yes/no

        ;lda #11
        lda backgroundSpriteColor+LUTAddr-rasterLine
        sta $d027
        sta $d028
        sta $d029
        sta $d02a
        sta $d02b
        sta $d02c
        sta $d02d
        sta $d02e

        lda #%11111111
        sta $d01b		; sprite before background

dontDoSprites:

	cpx #(7+7-1+1)          ; -1 because "inx" moved to below
	bcc oki2                ; branch if less than
	ldx #0
	oki2:
	inx
	stx curIdx

	lda nextScroll
	sta $d011

	lda nextRaster
	sta $d012

	lda #$ff
	sta $d019     

	lda yscroll+LUTAddr-rasterLine, x
	sta nextScroll

	lda gradient-2+LUTAddr-rasterLine, x
	sta prevBorder

	lda gradient-1+LUTAddr-rasterLine, x
	sta nextBorder

	lda rasterLine+LUTAddr-rasterLine, x
	clc
	adc skipFrameCounter

	sta nextRaster

	pla           
	tay
	pla           
	tax
	pla    
	rti 

placeLogoSprites:

        lda #0
        ldx showLogo
        beq logoInvisible
        lda #%00011111
        logoInvisible:
        sta $d015

        lda #%11111111
        sta $d010	

        lda #%00000000
        sta $d01b	

        lda #35
        sta $d000	
        lda #57-4
        sta $d001	

        lda #10;(35+24)
        sta $d00a	
        lda #11;(35+24)
        sta $d00c	
        sta $d00e	
        lda #(57-4)
        sta $d00b	
        lda #(58-4)
        sta $d00d	
        sta $d00f	

        lda #35
        sta $d000	
        lda #57-4
        sta $d001	

        lda #(35+7)
        sta $d004	
        lda #(57-4+12)
        sta $d005	

        lda #(35+7+24)
        sta $d006	
        lda #(57-4+12)
        sta $d007	

        lda #(35+2)
        sta $d008	
        lda #(57-4)
        sta $d009	


        ; y-coord of 3 sprites
        lda #58-6
        sta $d003	

        lda #35+24
        sta $d002	

        lda spriteLogoColors+LUTAddr-rasterLine+0
        sta $d027	
        sta $d028

        lda spriteLogoColors+LUTAddr-rasterLine+1
        sta $d029	
        sta $d02a	
        lda spriteLogoColors+LUTAddr-rasterLine+2
        sta $d02b	

        sta $d02c	
        lda spriteLogoColors+LUTAddr-rasterLine+1
        sta $d02d	
        sta $d02e	

        lda spriteLogoColors+LUTAddr-rasterLine+4

        lda #128+56+0
        sta $07f8
        lda #128+56+1
        sta $07f9
        lda #128+56+2
        sta $07fa
        lda #128+56+3
        sta $07fb
        lda #128+56+4
        sta $07fc
        lda #128+56+5
        sta $07fd
        lda #128+56+6
        sta $07fe
        lda #128+56+7
        sta $07ff
        
        rts
.endproc


_detectPALNTSC:
; from: https://codebase64.org/doku.php?id=base:detect_pal_ntsc (Sokrates variant)
    SEI
    LDX #$00
w0: LDA $D012
w1: CMP $D012
    BEQ w1
    BMI w0
    AND #$03
    CMP #$03
    BNE detectionDone ; done for NTSC
    TAY
countCycles:
    INX
    LDA $D012
    BPL countCycles
    CPX #$5E  
	; VICE values: PAL-N=$6C PAL=$50
    ; so choose middle value $5E for check 
    BCC isPAL
    INY ; is PAL-N
isPAL:
    TYA
detectionDone:
	rts



;    TWW's Variant from https://codebase64.org/doku.php?id=base:detect_pal_ntsc
;    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
;    // Detect PAL/NTSC
;    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
;    // 312 rasterlines -> 63 cycles per line PAL        => 312 * 63 = 19656 Cycles / VSYNC  => #>76  %00
;    // 262 rasterlines -> 64 cycles per line NTSC V1    => 262 * 64 = 16768 Cycles / VSYNC  => #>65  %01
;    // 263 rasterlines -> 65 cycles per line NTSC V2    => 263 * 65 = 17095 Cycles / VSYNC  => #>66  %10
;    // 312 rasterlines -> 65 cycles per line PAL DREAN  => 312 * 65 = 20280 Cycles / VSYNC  => #>79  %11

.proc   _detectC64Model
.export _detectC64Model
_detectC64Model:
    sei

    ;// Use CIA #1 Timer B to count cycled in a frame
    lda #$ff
    sta $dc06
    sta $dc07  ;// Latch #$ffff to Timer B

    bit $d011
    bpl *-3    ;// Wait untill Raster > 256
    bit $d011
    bmi *-3    ;// Wait untill Raster = 0

    ldx #%00011001
    stx $dc0f  ;// Start Timer B (One shot mode (Timer stops automatically when underflow))

    bit $d011
    bpl *-3    ;// Wait untill Raster > 256
    bit $d011
    bmi *-3    ;// Wait untill Raster = 0

    sec
    sbc $dc07  ;// Hibyte number of cycles used
    and #%00000011
    rts
.endproc


; ntsc -4
MPLEX_Y = 98

rasterLine: ; 15 byte
	; index 0 to 6
	.BYTE  14, 28, 42, 48, 86
        .BYTE      MPLEX_Y-1, MPLEX_Y+21-1, MPLEX_Y+21*2-1-1, MPLEX_Y+21*3-1, MPLEX_Y+21*4-1, MPLEX_Y+21*5-1-1, MPLEX_Y+21*6-1,     252,   2,  17

yscroll: ; 15 byte
	; index 0 to 6
	.BYTE  27, 27, 27, 27, 27
        .BYTE  27,27,27,27,27,27,27,    27,  27+128,  27+128


	; index -1 to 5
	;  duplicated values before "gradient" starts
	.BYTE 12		; belongs to rasterline index #6
	.BYTE 15		; belongs to rasterline index #6
; offset = 32
gradient: ; 2 + 14 = 16 byte
	.BYTE  15, 12, 11,   0,  16
        .BYTE 16, 16, 16, 16, 16, 16,16,   11,  12

rasterLineNTSC: ; 15 byte
	; index 0 to 6
	.BYTE  14, 33, 43, 47, 86
        .BYTE      MPLEX_Y-1, MPLEX_Y+21-1, MPLEX_Y+21*2-1-1, MPLEX_Y+21*3-1, MPLEX_Y+21*4-1, MPLEX_Y+21*5-1-1, MPLEX_Y+21*6-1,     251, 255, 3

	; index 0 to 6
yscrollNTSC: ; 15 byte
	.BYTE  27, 27, 27, 27, 27
        .BYTE  27,27,27,27,27,27,27,    27,  27,  27+128*0



	; index -1 to 5
	;  duplicated values before "gradient" starts
	.BYTE 12		; belongs to rasterline index #6
	.BYTE 15		; belongs to rasterline index #6
; offset = 78
gradientNTSC: ; 16 byte
	.BYTE  15, 12, 11,   0,  16
        .BYTE 16, 16, 16, 16, 16, 16,16,   11,  12

spriteLogoColors: ; offset = 92
        .BYTE 5, 6, 1, 0, 3, 13

backgroundSpriteColor: ; offset = 97
        .BYTE 11

fadeTab:
;.BYTE 0, 15, 0, 14, 0,11, 0, 13, 0, 0, 0, 0, 11, 5, 6, 12
.BYTE 0, 15, 9, 14, 2, 11, 0, 13, 9, 0, 2, 0, 11, 5, 6, 12


_setupSpriteLogo:

        ; copy sprite data to RAM
        ldx #0
copy_sprite:
        lda _spritesLogo,x
        sta $2e00,x
        lda _spritesLogo+256,x
        sta $2f00,x
        inx
        bne copy_sprite

	lda #%00111111
	
        sta $d015		; sprite on/off
	lda #%00000000
	sta $d017		; double height
	lda #%00000000
	sta $d01d		; double width

	lda #%11111111
	sta $d010		; sprite 0-7, most-significant bit x-coord

	lda #%00000000
	sta $d01b		; sprite before background

	lda #%00000000
	sta $d01c		; multicolor yes/no

	lda #(35+24)
	sta $d00a			; sprite 0, x
	lda #(57-4)
	sta $d00b			; sprite 0, y

	lda #35
	sta $d000			; sprite 0, x
	lda #57-4
	sta $d001			; sprite 0, y

	lda #(35+7)
	sta $d004			; sprite 0, x
	lda #(57-4+12)
	sta $d005			; sprite 0, y

	lda #(35+7+24)
	sta $d006			; sprite 0, x
	lda #(57-4+12)
	sta $d007			; sprite 0, y

	lda #(35+2)
	sta $d008			; sprite 0, x
	lda #(57-4)
	sta $d009			; sprite 0, y


	; y-coord of 3 sprites
	lda #58-4
	sta $d003		; sprite 0, y
	sta $d00d			; sprite 0, y

	lda #57-4
	sta $d00f			; sprite 0, y


	; x-coords of 3 sprites
	lda #68 
	sta $d00e			; sprite 1, x

	lda #35+24-8 
	sta $d002			; sprite 1, x

	lda #(68+12-3) 
	sta $d00c			; sprite 1, x

rts

_spritesLogo:
; grün, links ('S' und 'I')
.byte %00100000, %00000000, %00000000
.byte %00000000, %00000000, %00000000
.byte %00000100, %00000100, %00000100
.byte %01001000, %00000000, %00010000
.byte %01000001, %00000000, %00000000
.byte %00000001, %00000000, %00001001
.byte %00001000, %01000010, %01000000
.byte %00000100, %00100010, %01000000
.byte %10000001, %00000000, %00001001
.byte %10000001, %00000100, %00000000
.byte %00000000, %00101000, %00000001
.byte %00000000, %01000000, %00110000
.byte %00000000, %00000000, %00000000
.byte %10000000, %00000000, %00000000
.byte %00000000, %00000000, %00000000
.byte %00000000, %00000000, %00000000
.byte %00000000, %00000000, %00000000
.byte %00000000, %00000000, %00000000
.byte %00000000, %00000000, %00000000
.byte %00000000, %00000000, %00000000
.byte %00000000, %00000000, %00000000
.byte 0

;grün rechts (ab Mitte 'D' und 'E')
.byte %00000000, %00000000, %00000000
.byte %00000000, %00000000, %10000000
.byte %00000000, %00000000, %00000000
.byte %00100001, %00000000, %00000000
.byte %00000000, %00000100, %00000000
.byte %00001000, %00100000, %10000000
.byte %00000000, %00000000, %10000000
.byte %00100010, %00000000, %00000000
.byte %00001010, %00010000, %00000000
.byte %01001000, %01000001, %00000000
.byte %10000000, %00000001, %00000000
.byte %00010100, %00000000, %00000000
.byte %01000100, %00001000, %00000000
.byte %00000000, %00000000, %00000000
.byte %00000000, %00000000, %00000000
.byte %00000000, %00000000, %00000000
.byte %00000000, %00000000, %00000000
.byte %00000000, %00000000, %00000000
.byte %00000000, %00000000, %00000000
.byte %00000000, %00000000, %00000000
.byte %00000000, %00000000, %00000000
.byte 0

; blau 12px tiefer als grün
;blau, links
.byte %00000000, %00000000, %00000000
.byte %00000000, %00000000, %00000000
.byte %00001001, %01000100, %00000001
.byte %00001001, %01010000, %00010100
.byte %00000010, %10010000, %00000000
.byte %01000010, %00100000, %00001001
.byte %01010000, %00100010, %01000010
.byte %00010000, %01000010, %01000000
.byte %00000000, %01000000, %00000010
.byte %10000001, %00100100, %00001001
.byte %10100100, %00001000, %00000100
.byte %00100100, %10100000, %00100010
.byte %00000000, %00000000, %00000000
;.byte %10000000, %00000001, %00010010
.byte %10000000, %00000000, %00000000
.byte %00000000, %00000000, %00000000
.byte %00000000, %00000000, %00000000
.byte %00000000, %00000000, %00000000
.byte %00000000, %00000000, %00000000
.byte %00000000, %00000000, %00000000
.byte %00000000, %00000000, %00000000
.byte %00000000, %00000000, %00000000
.byte 0


;blau rechts
.byte %00000000, %00000000, %01000000
.byte %00000000, %00000000, %00000000
.byte %00000001, %00101000, %00000000
.byte %00000001, %00101010, %00000000
.byte %10000000, %01010010, %01000000
.byte %00001000, %01000100, %01000000
.byte %00000010, %00000100, %00000000
.byte %00000010, %00001000, %00000000
.byte %00000000, %00001000, %10000000
.byte %00000000, %00100100, %10000000
.byte %00010100, %10000000, %00000000
.byte %00010100, %10010100, %00000000
.byte %00000000, %00000001, %00000000
;.byte %10101101, %11011110, %00000000
.byte %00000000, %00000011, %00000000
.byte %00000000, %00000000, %00000000
.byte %00000000, %00000000, %00000000
.byte %00000000, %00000000, %00000000
.byte %00000000, %00000000, %00000000
.byte %00000000, %00000000, %00000000
.byte %00000000, %00000000, %00000000
.byte %00000000, %00000000, %00000000
.byte 0

; weiss, links
.byte %01110111, %01101010, %10000000
.byte %11000000, %00000000, %00000000
.byte %10000000, %00000000, %00000000
.byte %10000000, %00000000, %00000000
.byte %00000000, %00000000, %00000000
.byte %00000000, %00000000, %00000000
.byte %00000000, %00000000, %00000000
.byte %00000000, %00000000, %00000000
.byte %00000000, %00000000, %00000000
.byte %00000000, %00000000, %00000000
.byte %00000000, %00000000, %00000000
.byte %00000000, %00000000, %00000000
.byte %00000000, %00000000, %00000000
.byte %00000000, %11110111, %01101010
.byte %00000001, %10000000, %00000000
.byte %00000001, %00000000, %00000000
.byte %00000000, %00000000, %00000000
.byte %00000000, %00000000, %00000000
.byte %00000000, %00000000, %00000000
.byte %00000000, %00000000, %00000000
.byte %00000000, %00000000, %00000000
.byte 0

.byte %00001111, %11000011, %10001110
.byte %00011111, %11000011, %10011100
.byte %00111111, %10000111, %00011100
.byte %01111000, %00000111, %00011100
.byte %01111111, %10000111, %00011100
.byte %01111111, %11000111, %00111100
.byte %01111111, %11001111, %11111000
.byte %01110001, %11001111, %11111000
.byte %11110011, %11000111, %11111000
.byte %11111111, %10000000, %00111000
.byte %01111111, %10000000, %01111000
.byte %00111110, %00000000, %01110000
.byte %00000000, %00000000, %00000000
.byte %00000000, %00000000, %00000000
.byte %00000000, %00000000, %00000000
.byte %00000000, %00000000, %00000000
.byte %00000000, %00000000, %00000000
.byte %00000000, %00000000, %00000000
.byte %00000000, %00000000, %00000000
.byte %00000000, %00000000, %00000000
.byte %00000000, %00000000, %00000000
.byte 0

.byte %00010000, %00100100, %00010000
.byte %00100000, %00000100, %00000010
.byte %01000000, %01000000, %10100010
.byte %00000000, %00001000, %10100000
.byte %00000000, %01001000, %10100000
.byte %00000000, %00101000, %00000000
.byte %10000000, %00100000, %00000100
.byte %10001010, %00000000, %00000100
.byte %00000000, %00001000, %00000000
.byte %00000000, %01000000, %01000000
.byte %10000000, %00000000, %00000000
.byte %01000001, %00000000, %00001000
.byte %00000000, %00000000, %00000000
.byte %00000000, %00000000, %00000000
.byte %00000000, %00000000, %00000000
.byte %00000000, %00000000, %00000000
.byte %00000000, %00000000, %00000000
.byte %00000000, %00000000, %00000000
.byte %00000000, %00000000, %00000000
.byte %00000000, %00000000, %00000000
.byte %00000000, %00000000, %00000000
.byte 0




; peak meter bar
.byte %11110111, %00111101, %00101111
.byte %10000100, %10100001, %00101000
.byte %11100100, %10111001, %10101110
.byte %10000101, %00100001, %01101000
.byte %10000100, %10111101, %00101111
.byte %00000000, %00000000, %00000000
.byte %00000000, %00000000, %00000000
.byte %00000000, %00000000, %00000000
.byte %11111011, %10011100, %00000000
.byte %00100001, %00100000, %00000000
.byte %00100001, %00100000, %00000000
.byte %00100001, %00100000, %00000000
.byte %00100011, %10011100, %00000000
.byte %00000000, %00000000, %00000000
.byte %00000000, %00000000, %00000000
.byte %00000000, %00000000, %00000000
.byte %00000000, %00000000, %00000000
.byte %00000000, %00000000, %00000000
.byte %00000000, %00000000, %00000000
.byte %00000000, %00000000, %00000000
.byte %00000000, %00000000, %00000000
.byte 0

.byte %11110111, %00111101, %00101111
.byte %10000100, %10100001, %00101000
.byte %11100100, %10111001, %10101110
.byte %10000101, %00100001, %01101000
.byte %10000100, %10111101, %00101111
.byte %00000000, %00000000, %00000000
.byte %00000000, %00000000, %00000000
.byte %00000000, %00000000, %00000000
.byte %11111011, %10011100, %00000000
.byte %00100001, %00100000, %00000000
.byte %00100001, %00100000, %00000000
.byte %00100001, %00100000, %00000000
.byte %00100011, %10011100, %00000000
.byte %00000000, %00000000, %00000000
.byte %00000000, %00000000, %00000000
.byte %00000000, %00000000, %00000000
.byte %00000000, %00000000, %00000000
.byte %00000000, %00000000, %00000000
.byte %00000000, %00000000, %00000000
.byte %00000000, %00000000, %00000000
.byte %00000000, %00000000, %00000000
.byte 0
.byte 0

.byte %00000000, %00000000, %00000000
.byte %00000000, %00000000, %00000000
.byte %00000000, %00000000, %00000000
.byte %00000000, %00000000, %00000000
.byte %00000000, %00000000, %00000000
.byte %00000000, %00000000, %00000000
.byte %00000000, %00000000, %00000000
.byte %00000000, %00000000, %00000000
.byte %00000000, %00000000, %00000000
.byte %00000000, %00000000, %00000000
.byte %00000000, %00000000, %00000000
.byte %00000000, %00000000, %00000000
.byte %00000000, %00000000, %00000000
.byte %00000000, %00000000, %00000000
.byte %00000000, %00000000, %00000000
.byte %00000000, %00000000, %00000000
.byte %00000000, %00000000, %00000000
.byte %00000000, %00000000, %00000000
.byte %00000000, %00000000, %00000000
.byte %00000000, %00000000, %00000000
.byte %00000000, %00000000, %00000000
.byte 0

.macro SHL_YX
        txa
        clc
        rol
        tax
        tya
        rol
        tay
.endmacro

; compute _a * _b + _c:_d
; store result in $27:$26
; uses $fa-$fd
.macro mad16 _a, _b, _c, _d
.local loop, no_add
        lda _a
        sta $fb
        lda _b
        sta $fc
        lda _c
        sta $fa
        lda _d
        sta $fd

	;//	c_ * y_
        ;c_ = $fb
        ;y_ = $fc
        ;x_ = $fd
	lda #0
	ldx #$8
	lsr $fb
	loop:
	bcc no_add
	clc
	adc $fc
	no_add:
	ror
	ror $fb
	dex
	bne loop
	sta $fc

	;//	... + x_
	lda $fb
	clc
	adc $fd
	sta $fb
	lda $fc
	adc #0
	sta $fc


	lda $fb
	sta $26
	lda $fc
	clc
	;adc #$d8
        adc $fa
	sta $27
.endmacro

.proc   _transferScreenVDC
.export _transferScreenVDC
_transferScreenVDC:
        mad16 #40, lastVDCUpdateLine, #$04, #0

        lda $26
        sta $fb
        lda $27
        sta $fc

        ; VDC screen starts at address (0,0) => use screen RAM address - $0400
        lda $26
        ldx $27
        dex
        dex
        dex
        dex

        ;// set write address
        ldy #19
        sty $d600
        dey ; == #18
        wait2:
        bit $d600
        bpl wait2
        sta $d601

        sty $d600
        wait1:
        bit $d600
        bpl wait1
        stx $d601

        ; copy loop
        ldx #31
        ldy #0
      cpl:
        stx $d600
        lda ($fb),y
        iny
      wait:
        bit $d600
        bpl wait
        sta $d601
        cpy #(40*5)     ; 5 lines, each 40 chars
        bne cpl


        ;// copy 5 lines of c64 color RAM

        ; VDC attributes starts at address (8,0) => use screen RAM address + $0400
        lda $26
        ldx $27
        inx
        inx
        inx
        inx

        ;// set write address
        ldy #19
        sty $d600
        dey ; == #18
        wait2a:
        bit $d600
        bpl wait2a
        sta $d601

        sty $d600
        wait1a:
        bit $d600
        bpl wait1a
        stx $d601

        ; color RAM addr = screen addr + (d8-04):00
        lda $26
        sta $fb
        lda $27
        clc
        adc #($d8-$04)
        sta $fc

        ; offset on color lookup table?
        lda $d018
        and #2
        bne copy_one_line_attr_charset2

      copy_one_line_attr:

        ;// convert 40 colors
        ldy #0
      cpl_attr:
        ldx #31
        stx $d600
        lda ($fb),y
        iny
        tax
        lda $5400,x     ; vdcColorConversion 

      wait_attr:
        bit $d600
        bpl wait_attr
        sta $d601

        cpy #(40*5)     ; 5 lines, each 40 chars
        bne cpl_attr

        rts

      copy_one_line_attr_charset2:

        ;// convert 40 colors
        ldy #0
      cpl_attr2:
        ldx #31
        stx $d600
        lda ($fb),y
        iny
        tax
        lda $5500,x     ; vdcColorConversion 

      wait_attr2:
        bit $d600
        bpl wait_attr2
        sta $d601

        cpy #(40*5)     ; 5 lines, each 40 chars
        bne cpl_attr2

        rts

.endproc
