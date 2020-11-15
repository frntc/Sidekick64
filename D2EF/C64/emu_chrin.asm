.pc = * "CHRIN"
	DO_CHRIN:

		// in: -
		// used: A
		// out: A, C

		// setup exit
		:mov #$4c ; RETURN_COMMAND
		:mov16 REAL_CHRIN ; RETURN_ADDR

		// check active device
		lda $99
		bne !just_rts+ // other device = just the orig. routine

	!keyboard:
		lda $d0
		bne !just_rts+

		sty TEMP1

jmp get_out
		lda AUTOTYPE_STATUS
		bne !line1+
	!line2:
		ldy #[autotype_line2 - autotype_line1]
		.byte $2c
	!line1:
		ldy #0

	!loop:
		lda autotype_line1, y
		beq !new_line+
		jsr $e716 // print char
		inc $c8
		iny
		bne !loop-

	!new_line:
		sta $0292
		sta $d3
		sta $d4
		inc $d0

	!exit_chrin:
		ldy TEMP1
		dec AUTOTYPE_STATUS
		bpl !just_rts+

get_out:
		// disable CHRIN
		:mov16 REAL_CHRIN ; $0324

	!just_rts:
		jmp DISABLE_CRT

autotype_line1:
	.byte $4c, $4f, $41, $44, $22, $2a, $22, $2c, $38, $2c, $31, 0
autotype_line2:
	.byte $52, $55, $4e, 0
