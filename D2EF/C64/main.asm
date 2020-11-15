/*
	java -jar KickAss.jar kapi_nm.asm -showmem -vicesymbols
*/

.const MODE_RAM = $04
.const MODE_16k = $07
.const MODE_8k  = $06
.const MODE_ULT = $05

.const IO_MODE = $de02

.var DIR = DISPLAY+40



/*
** and now some often used variables
*/

// points to the current entry in the dir (after FIND_FILE)
.const ENTRY_PTR = $fb // $fc
.const TEMP1 = $fd
.const TEMP2 = $fe
.const TEMP3 = $ff

.const ZP_SIZE = 5

// during LOAD
.const ZP_BANK = TEMP1
.const SRC_PTR = TEMP2 // TEMP3
.const SIZE = ENTRY_PTR
.const DEST_PTR = $ae // $af

.const fast_load = true


.import source "helper/commands.asm"
.import source "helper/commands8.asm"
.import source "helper/commands16.asm"
.import source "helper/macros.asm"

.pc = BANK_START "Init"

.if(! ULTIMAX){
	.word start
	.word start
	.byte $c3, $c2, $cd, $38, $30 // CBM80
start:
}

	// quick init
	sei
	ldx #$ff
	txs
	cld
	stx $8004 // no CBM80
	:mov #$e7 ; $01
	:mov #$2f ; $00

	lda #MODE_16k|$80
	sta IO_MODE

	// copy reset-routine
	ldx #[RESET_END - RESET_START]+1
!loop:
	lda RESET_START-1, x
	sta $df02-1, x
	dex
	bne !loop-

	// activate reset-routine
	jmp $df02

.pc = * "Reset routine"

	// the reset-routine
RESET_START:
	.pseudopc $df02 {
		/*
			PARTIAL RESET
		*/

		// disable rom
		lda #MODE_RAM
		sta IO_MODE

		// do a partial reset
		ldx #$05
		stx $d016
		jsr $fda3
		jsr $fd50
		jsr $fd15
		jsr $ff5b

		// change CHRIN vector
		:mov16 $324 ; ORIG_CHRIN_VECTOR

		// add our vector
		:mov16 #RESET_TRAP ; $324

		/*
			RESET, PART 2
		*/

	GO_RESET:
		jmp $fcfe

	/*
	** RESET IS DONE
	** RESTORE VECTOR
	** JUMP BACK IN CARTRIDGE
	*/

	RESET_TRAP:
		// disable interrups
		sei

		// enable rom
		lda #MODE_16k|$80
		sta IO_MODE

		// install
		jmp INSTALL_API

	ORIG_CHRIN_VECTOR:
	}
RESET_END:

.print "size 'reset' = " + [RESET_END - RESET_START]

.pc = * "Install Vectors"

INSTALL_API:
	// keep x (in $02)
	stx $02

	ldx #39
!loop:
	lda launchmessage, x
	sta $0400+1*40, x
	lda lower_line, x
	sta $0400+3*40, x
	lda #WHITE
	sta $d800+1*40, x
	sta $d800+3*40, x
	dex
	bpl !loop-

	// save orig.chrin vector
	:mov16 ORIG_CHRIN_VECTOR ; $fe

	// copy reset-routine
	ldx #[MAIN_END - MAIN_START]+1
!loop:
	lda MAIN_START-1, x
	sta $df02-1, x
	dex
	bne !loop-

	// save orig.vector
	:mov16 $fe ; REAL_CHRIN

	// copy vectors
	:mov16 $0330 ; REAL_LOAD

	// setup new vectors
	:mov16 #VECTOR_CHRIN ; $0324
	:mov16 #VECTOR_LOAD ; $0330

	// use "lda #$xx" instead of "lda $xxxx"
	:mov $df00 ; SMC_OUR_BANK+1

	// restore X
	ldx $02

	// continue CHRIN
	jmp VECTOR_CHRIN

lower_line:
	.import binary "copyright.bin"
	//.byte $20, $02, $19, $20, $01, $0c, $05, $18, $27, $13, $20, $0d, $32, $09, $2d, $14, $0f, $2d, $05, $01, $13, $19, $06, $0c, $01, $13, $08, $20, $22, $0c, $0f, $01, $04, $22, $20, $16, $30, $2e, $37, $20

.pc = * "Vectors"

MAIN_START:
	.import source "emu_vectors.asm"
MAIN_END:

.print "size 'new functions' = " + [MAIN_END - MAIN_START]

.import source "emu_chrin.asm"
.import source "emu_helper.asm"
.import source "emu_load.asm"

launchmessage:
.text "   sidekick64 disk image launch using   "

DISPLAY:
//     0123456789012345678901234567890123456789
