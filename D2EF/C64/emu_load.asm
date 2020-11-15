.pc = * "LOAD"
	DO_LOAD:

		// check weather load is active
		lda $93
		bne use_real_load // we do'nt support verify -> use real load

		// check drive#
		lda $ba
		cmp #$08
		beq find_file // if drive is not 8 -> use real load

	use_real_load:

		// failure+return
		:mov #$4c ; RETURN_COMMAND
		:mov16 REAL_LOAD ; RETURN_ADDR

		lda $93
		ldx $c3
		ldy $c4

		jmp DISABLE_CRT


		// search file - exit if not found
	find_file:
		jsr FIND_FILE
		bcs use_real_load

		// print SEARCHING
		:mov16 #$f5af ; SMC_PRINT_SEARCH_LOADING+1
		jsr PRINT_SEARCH_LOADING
		// wait, if it's the first time
		lda AUTOTYPE_STATUS
		bmi !skip+
		ldx #0
		ldy #0
	!loop:
		lda ($00, x)
		lda ($00, x)
		lda ($00, x)
		lda ($00, x)
		inx
		bne !loop-
		iny
		bne !loop-
	!skip:
		// print LOADING
		:mov16 #$f5d2 ; SMC_PRINT_SEARCH_LOADING+1
		jsr PRINT_SEARCH_LOADING


		ldy #16
		// copy bank
		lda (ENTRY_PTR), y
		clc
		adc $df00
.if(!fast_load){
		sta SMC_BANK+1
}else{
		sta ZP_BANK
}
		iny
		iny // upper bank-register
		// copy offset
		lda (ENTRY_PTR), y
.if(!fast_load){
		sta SMC_ADDR+1
}else{
		sta SRC_PTR+0
}
		iny
		lda (ENTRY_PTR), y
.if(!fast_load){
		sta SMC_ADDR+2
}else{
		sta SRC_PTR+1
}
		iny
		// copy load-address
		lda (ENTRY_PTR), y
		sta DEST_PTR+0
		iny
		lda (ENTRY_PTR), y
		sta DEST_PTR+1
		iny
		// copy size
		lda (ENTRY_PTR), y
		pha
		iny
		lda (ENTRY_PTR), y
		sta SIZE+1
		//iny
		pla
		sta SIZE+0

		// load-address
		lda $b9
		bne copy_file

	use_own_la:
		// use the laodaddress specified
		:mov $c3 ; DEST_PTR+0
		:mov $c4 ; DEST_PTR+1

	copy_file:
.if(!fast_load){
		// dec 1, because we count to underflow
		:dec16 SIZE

		ldy #0
	!loop:
		jsr READ_BYTE
		sta (DEST_PTR), y
		:inc16 DEST_PTR
		lda SIZE+0
		bne nodecupper
		lda SIZE+1
		beq !exit+
		dec SIZE+1
	nodecupper:
		dec SIZE+0
		jmp !loop-

	!exit:
		// success+return
		ldx DEST_PTR+0
		ldy DEST_PTR+1
		clc
		:mov #$60 ; RETURN_COMMAND

		jmp DISABLE_CRT


}else{


	// update size (for faked start < 0)
	:add16_8 SIZE ; SRC_PTR

	// lower source -> y ; copy always block-wise
	:sub16_8 DEST_PTR ; SRC_PTR
	ldy SRC_PTR
	:mov #0 ; SRC_PTR
	sta smc_limit+1

	:if SIZE+1 ; NE ; #$00 ; JMP ; COPY_FILE
	sty smc_limit+1
	jmp COPY_FILE_LESS_THEN_ONE_PAGE

}

/*

	fast:
  ADDR AC XR YR SP 00 01 NV-BDIZC LIN CYC
.;dfc6 60 00 3a f7 2f 37 00110100 000 004

	slow:
  ADDR AC XR YR SP 00 01 NV-BDIZC LIN CYC
.;82db 60 00 3a f7 2f 37 00110100 000 004

*/
