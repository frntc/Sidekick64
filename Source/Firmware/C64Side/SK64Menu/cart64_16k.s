# this file has been adapted from the great example found here:
# http://swut.net/c64cart-c.html
# ... plus a little bit of initialization calls for Sidekick added.
;
; Startup code for cc65 (C64 version)
;

	.export		_exit
        .export         __STARTUP__ : absolute = 1      ; Mark as startup
	.import		initlib, donelib, callirq
       	.import	       	zerobss
	.import	     	callmain, pushax
        .import         RESTOR, BSOUT, CLRCH
	.import	       	__INTERRUPTOR_COUNT__
	.import		__RAM_START__, __RAM_SIZE__	; Linker generated

	.import		_cgetc, _puts, _memcpy

	.import		__DATA_LOAD__, __DATA_RUN__, __DATA_SIZE__

	.include        "zeropage.inc"
	.include     	"c64.inc"

.import _main
.import _loop
.import _myInitVideo

.segment	"HEADERDATA"

HeaderB:
@magic:
	;.byt	"C64  CARTRIDGE  "
	.byt	$43,$36,$34,$20, $43,$41,$52,$54
	.byt	$52,$49,$44,$47, $45,$20,$20,$20
@headelen:
	.byt	$00,$00,$00,$40
@ver:
	.byt	$01,$00
@carttye:
	.byt	$00,$00
@EXROM:
	.byt	$01
@GAME:
	.byt	$01
@reserved1:
	.byt	$00,$00,$00,$00,$00,$00
@Name:	;You put the name of the cartridge here.
	.byt	"SIDEKICK "


.segment	"CHIP0"
ChipB:
@magic:
	.byt	$43,$48,$49,$50 ;"CHIP"
@size:
	.byt	$00,$00,$20,$10	;Use for 8k carridge
	;.byt	$00,$00,$40,$10	;Use for 16k carridge
@chiptype:	;ROM
	.byt	$00,$00
@bank:
	.word	$0000
@start:
	.byt	$80,$00
@size2:
	.byt	$40,$00

.segment	"STARTUP"
	.word	startup		;Cold start
	.word	$FEBC		;Warm start, default calls NMI exit.
	.byt	$C3,$C2,$CD,$38,$30 ;magic to identify cartridge

startup:
	lda #0
	sta $d020
	sta $d021
	lda #$0b
	sta $d011
	jsr	$FF84		;Init. I/O
	lda #0
	sta $d011

	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
	; initalise memory pointers

	LDA #$00
	TAY
	lFD53:
	STA $0002,Y
	STA $0200,Y
	STA $0300,Y
	INY
	BNE lFD53
	LDX #$3C
	LDY #$03
	STX $B2
	STY $B3

	lFD88:
	CLC
	LDA #$00
	STA $0283 ; 00
	LDA #$A0
	STA $0284 ; a0
	LDA #$08
	STA $0282
	LDA #$04
	STA $0288
	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

	lda #0
	sta $d011
	jsr	$FF8A		;Restore vectors
	jsr _myInitVideo

	lda #0
	sta $d011
	sta $d020
	sta $d021

	lda	#14
	jsr	BSOUT

	; Clear the BSS data
	jsr	zerobss

	lda    	#<(__RAM_START__ + __RAM_SIZE__)
	sta	sp
	lda	#>(__RAM_START__ + __RAM_SIZE__)
   	sta	sp+1   		; Set argument stack ptr

NoIRQ1:
	lda	#<__DATA_RUN__
	ldx	#>__DATA_RUN__
	jsr	pushax
	lda	#<__DATA_LOAD__
	ldx	#>__DATA_LOAD__
	jsr	pushax
	lda	#<__DATA_SIZE__
	ldx	#>__DATA_SIZE__
	jsr	_memcpy
	
	jsr	initlib

    jsr     callmain

	; Back from main (This is also the _exit entry). Run module destructors

_exit:  jsr	donelib


; Reset the IRQ vector if we chained it.
NoIRQ2:	
	lda	#<exitmsg
	ldx	#>exitmsg
	jsr	_puts
	jsr	_cgetc
	jmp	64738		;Kernal reset address as best I know it.

.rodata
exitmsg:
	.byt	"Your program has ended.  Press any key",13,"to continue...",0

.segment "CODE2" 

.segment "CODE" 



