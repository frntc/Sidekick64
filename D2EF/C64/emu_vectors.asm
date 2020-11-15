
.pseudopc $df02 {

	/*
	** LOAD emulation (vector)
	*/

	VECTOR_LOAD:
		// save REGS
		sta $93
//		stx $c3 - already done before calling
//		sty $c4 - already done before calling

		jsr copy_filename
		jmp DO_LOAD

	/*
	** CHRIN emulation (vector)
	*/

	VECTOR_CHRIN:
		jsr ENABLE_CRT
		jmp DO_CHRIN

	/*
	** Variables
	*/

	REAL_CHRIN:
		.fill 2, 0
	REAL_LOAD:
		.fill 2, 0

	AUTOTYPE_STATUS:
		.byte $01

	FILENAME:
		.fill 16, 0


	/*
	** read a byte
	*/

.if(!fast_load){
	RAM_READ_BYTE:
	SMC_BANK:
		lda #$00
		sta $de00
	SMC_ADDR:
		lda $ffff
		stx $de00
		rts
}

	copy_filename:
		// copy filename
		ldy #15
	!loop:
		lda ($bb), y
		sta FILENAME, y
		dey
		bpl !loop-

//		jmp ENABLE_CRT // return this subroutine

	ENABLE_CRT:
		pha
		// no interrupts
		sei
		// remember 01 setting
		lda $01
		sta smc_one+1
		// enable basic (cart), kernal (should already be) and i/o (should already be)
		ora #$07
		sta $01
		// backup ZP-VARS
		txa
		pha
		ldx #ZP_SIZE-1
	!loop:
		lda $100 - ZP_SIZE, x
		sta BACKUP_ZP, x
		dex
		bpl !loop-
		pla
		tax
		// enable rom
		lda #MODE_16k|$80
		sta IO_MODE
		// jump to home bank
	SMC_OUR_BANK:
		lda #$00
		sta $de00
		//
		pla
		rts

	PRINT_SEARCH_LOADING:
		// disable rom
		lda #MODE_RAM
		sta IO_MODE
		// call kernal
	SMC_PRINT_SEARCH_LOADING:
		jsr $ffff
		// enable rom
		lda #MODE_16k|$80
		sta IO_MODE
		rts

.if(fast_load){

	/*
		do the file-copy
	*/

	add_bank:
		:mov #BANK_START >> 8 ; SRC_PTR+1
		inc ZP_BANK
	COPY_FILE:
		lda ZP_BANK
		sta $de00
	!loop:
		lda (SRC_PTR), y
		sta (DEST_PTR), y
		iny
		bne !loop-
		inc DEST_PTR+1
		inc SRC_PTR+1
		dec SIZE+1
		beq !skip+
		:if SRC_PTR+1 ; EQ ; #[BANK_START+BANK_SIZE] >> 8; add_bank
		jmp !loop-
end_of_full_page:
	!skip:
		:if SRC_PTR+1 ; EQ ; #[BANK_START+BANK_SIZE] >> 8 ; ENDIF ; !endif+
			:mov #BANK_START >> 8 ; SRC_PTR+1
			inc ZP_BANK
	COPY_FILE_LESS_THEN_ONE_PAGE:
			lda ZP_BANK
			sta $de00
		!endif:
		ldy SIZE
		beq !skip+
	!loop:
		dey
		lda (SRC_PTR), y
		sta (DEST_PTR), y
	smc_limit:
		cpy #$00
		bne !loop-

	!skip:

		// setup end of program
		:add16_8 DEST_PTR ; SIZE

		// success+return
		ldx DEST_PTR+0
		ldy DEST_PTR+1
		clc
		:mov #$60 ; RETURN_COMMAND
	//	jmp DISABLE_CRT -- is done due to next address


}

	DISABLE_CRT:
		php
		pha
		// restore ZP-VARS
		txa
		pha
		ldx #ZP_SIZE-1
	!loop:
		lda BACKUP_ZP, x
		sta $100 - ZP_SIZE, x
		dex
		bpl !loop-
		pla
		tax
		// disable rom
		lda #MODE_RAM
		sta IO_MODE
		// restore 01 setting
	smc_one:
		lda #$00
		sta $01
		//
		pla
		plp
		// interrupts
		cli
	RETURN_COMMAND:
		.fill 1, 0
	RETURN_ADDR:
		.fill 2, 0

	BACKUP_ZP:
		.fill ZP_SIZE, $ff




}
