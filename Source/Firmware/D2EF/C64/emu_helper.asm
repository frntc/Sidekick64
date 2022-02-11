.pc = * "Helpers"

.if(!fast_load){
	READ_BYTE: // TRASHES X!!
		:if SMC_ADDR+2 ; EQ ; #[[BANK_START + BANK_SIZE] >> 8] ; ENDIF ; !endif+
			// go to next bank
			:mov #BANK_START>>8 ; SMC_ADDR+2
			inc SMC_BANK+1
		!endif:
		ldx SMC_OUR_BANK+1
		jsr RAM_READ_BYTE
		:inc16 SMC_ADDR+1
		:dec16 OPEN_SIZE_TODO ; X
		rts
}

	// temporary variables
	.const FILENAME_LEN = $bf

	FIND_FILE:
		// requires that the filename is copied to FILENAME
		// return C=error

		// y=len of filename
		ldy $b7

		// check for empty filename
		beq file_not_found

		// if len > 16 chars: fail
		:if Y ; GT ; #16 ; use_real_load

		// find last non-space char
	!loop:
		dey
		lda FILENAME, y
		cmp #$20
		beq !loop-
		// remember length
		iny
		sty FILENAME_LEN

		// find the entry
		:mov16 #DIR-24 ; ENTRY_PTR
	find_loop:
		:add16_8 ENTRY_PTR ; #24
		ldy #0
		lda (ENTRY_PTR), y
		beq file_not_found // EOF = file not found

	!loop:
		lda FILENAME, y
		cmp #$2a // '*'
		beq file_found // match (due to a *) -> found file
		cmp (ENTRY_PTR), y
		bne find_loop // mismatch -> next line

		iny // next char
		cpy FILENAME_LEN
		bne !loop- // not at enf of name -> next char

		cpy #16
		beq !skip+
		// found a file whith <16 chars -> check for \0 as next char in name
		lda (ENTRY_PTR), y
		bne find_loop // filenamelen mismatch
	!skip:

	file_found:
		clc
		rts

	file_not_found:
		sec
		rts



	RETURN_SUCCESS:
		clc
		:mov #$60 ; RETURN_COMMAND
		jmp DISABLE_CRT
