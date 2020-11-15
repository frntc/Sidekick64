	// dont load the bank, its X. if bank 1 then with INX before
	.pc = $ffe5 "jumper"

	jump_start:
		// store bank
		lda #$07 // 16k-mode
		sta $de02
		jmp $a000 // jump into the cartridge

	.pc = * "copy it"

	start:
		sei
		// since no calculations are made and the stack is not used we do'nt initialize them
		ldx #[start-jump_start]
	// first, initialize ram
	// second, copy a few bytes to $02+
	loop:
		lda jump_start-1, x
		sta $02-1, x
		dex
		bne loop

		// X is now zero
		beq $10002

	.pc = $fffa "vectors"
		.word do_rti
		.word start // @fffc -> address of reset routine
	do_rti:
		rti
		.byte $78
