/*
** 16bit commands
*/

.pseudocommand clr16 dst ; hop {
	.if(_isregister16(dst)){
		:_load _16bit_lowerArgument(dst) ; #0
		:_load _16bit_upperArgument(dst) ; #0
	}else{
		.if(_isunset(hop)){
			.eval hop = A
		}else .if(! _isregister8(hop)){
			.error "clr16: hop is not a register"
		}
		:_load hop ; #0
		:_store hop ; _16bit_lowerArgument(dst)
		:_store hop ; _16bit_upperArgument(dst)
	}
} 
 
.pseudocommand mov16 src ; hop ; dst { 
	.if(_isunset(dst)){
		.eval dst = hop
		.eval hop = _DEFAULT
	}else .if(_isunset(hop)){
		.eval hop = _DEFAULT
	}else .if(! _isregister8(hop)){
		.error "mov16: hop is not a register"
	}
	:mov _16bit_lowerArgument(src) ; hop ; _16bit_lowerArgument(dst)
	:mov _16bit_upperArgument(src) ; hop ; _16bit_upperArgument(dst)
} 

.pseudocommand inc16 arg { 
		:inc _16bit_lowerArgument(arg) 
		bne over 
		:inc _16bit_upperArgument(arg) 
	over: 
} 
 
.pseudocommand dec16 arg ; hop {
		.if(_isunset(hop)){
			.eval hop = A
		}else .if(! _isregister8(hop)){
			.error "dec16: hop is not a register"
		}
		.if(_isregister16(arg)){
			:_compare _16bit_lowerArgument(arg) ; #0
		}else{
			:_load hop ; _16bit_lowerArgument(arg)
		}
		bne nodecupper
		:dec _16bit_upperArgument(arg)
	nodecupper:
		:dec _16bit_lowerArgument(arg) 
} 
 
.pseudocommand ror16 src ; dst {
	.if(_isunset(dst) /*|| dst.getValue() == src.getValue()*/){
		ror _16bit_upperArgument(src)
		ror _16bit_lowerArgument(src)
	}else{
		lda _16bit_upperArgument(src)
		ror
		sta _16bit_upperArgument(dst) 
		lda _16bit_lowerArgument(src)
		ror
		sta _16bit_lowerArgument(dst)
	}
}

.pseudocommand lsr16 src ; dst {
	.if(_isunset(dst) /*|| dst.getValue() == src.getValue()*/){
		lsr _16bit_upperArgument(src)
		ror _16bit_lowerArgument(src)
	}else{
		lda _16bit_upperArgument(src)
		lsr
		sta _16bit_upperArgument(dst) 
		lda _16bit_lowerArgument(src)
		ror
		sta _16bit_lowerArgument(dst)
	}
}

.pseudocommand rol16 src ; dst {
	.if(_isunset(dst) /*|| dst.getValue() == src.getValue()*/){
		rol _16bit_lowerArgument(src)
		rol _16bit_upperArgument(src)
	}else{
		lda _16bit_lowerArgument(src)
		rol
		sta _16bit_lowerArgument(dst)
		lda _16bit_upperArgument(src)
		rol
		sta _16bit_upperArgument(dst) 
	}
}

.pseudocommand asl16 src ; dst {
	.if(_isunset(dst) /*|| dst.getValue() == src.getValue()*/){
		asl _16bit_lowerArgument(src)
		rol _16bit_upperArgument(src)
	}else{
		lda _16bit_lowerArgument(src)
		asl
		sta _16bit_lowerArgument(dst)
		lda _16bit_upperArgument(src)
		rol
		sta _16bit_upperArgument(dst) 
	}
}

.pseudocommand adc16 src1 ; src2 ; dst { 
	.if(src2.getType() == AT_IMMEDIATE && src2.getValue() < 256){
		// src2 is immediate ("#xxx") and less than 256: use shorter command
		:adc16_8 src1 ; src2 ; dst
	}else{
		.if(_equalregisters8(src1, A)){
			// dst = A(8bit) + src2
			adc _16bit_lowerArgument(src2)
			sta _16bit_lowerArgument(dst)
			lda #0
			adc _16bit_upperArgument(src2)
			sta _16bit_upperArgument(dst)
/*
**	sorry, can't do this...
		}else .if(src1.getValue() == src2.getValue()){
			// (src1 | dst) = src1 + src1 = 2*src1 = src1<<1
			:rol16 src1 ; dst
*/
		}else{
			// (src1 | dst) = src1 + src2
			.if(_isunset(dst)){
				.eval dst = src1
			}
			lda _16bit_lowerArgument(src1)
			adc _16bit_lowerArgument(src2)
			sta _16bit_lowerArgument(dst)
			lda _16bit_upperArgument(src1)
			adc _16bit_upperArgument(src2)
			sta _16bit_upperArgument(dst)
		}
	}
}

.pseudocommand add16 src1 ; src2 ; dst {
/*
**	sorry, can't do this...
	.if(! _equalregisters8(src1, A) && src1.getValue() == src2.getValue()){
		// (src1 | dst) = src1 + src1 = 2*src1 = src1<<1
		:asl16 src1 ; dst
	}else{
*/
		clc
		:adc16 src1 ; src2 ; dst
//	}
}

.pseudocommand adc16_8 src1 ; src2 ; dst {
	.if(_equalregisters8(src1, A)){
		// dst = A(8bit) + src2(8bit)
			adc src2
			sta _16bit_lowerArgument(dst)
			adc #0
			sta _16bit_upperArgument(dst)
	}else .if(_isunset(dst) /*|| dst.getValue() == src1.getValue()*/){
		// (src1==dst) = src1 + src2(8bit)
			lda _16bit_lowerArgument(src1)
			adc src2
			sta _16bit_lowerArgument(src1)
			bcc skip
			inc _16bit_upperArgument(src1) 
		skip:
	}else{
		// dst = src1 + src2(8bit)
			lda _16bit_lowerArgument(src1)
			adc src2
			sta _16bit_lowerArgument(dst)
			lda _16bit_upperArgument(src1) 
			adc #0
			sta _16bit_upperArgument(dst) 
	}
}

.pseudocommand add16_8 src1 ; src2 ; dst { 
	clc
	:adc16_8 src1 ; src2 ; dst
}

.pseudocommand sbc16 src1 ; src2 ; dst {
		lda _16bit_lowerArgument(src1)
		sbc _16bit_lowerArgument(src2)
		sta _16bit_lowerArgument(dst)
		lda _16bit_upperArgument(src1)
		sbc _16bit_upperArgument(src2)
		sta _16bit_upperArgument(dst)
}

.pseudocommand sub16 src1 ; src2 ; dst {
	sec
	:sbc16 src1 ; src2 ; dst
}

.pseudocommand sbc16_8 src1 ; src2 ; dst {
	.if(_isunset(dst)){
			lda _16bit_lowerArgument(src1)
			sbc _16bit_lowerArgument(src2)
			sta _16bit_lowerArgument(src1)
			bcs !skip+
			dec _16bit_upperArgument(src1)
		!skip:
	}else{
			lda _16bit_lowerArgument(src1)
			sbc _16bit_lowerArgument(src2)
			sta _16bit_lowerArgument(dst)
			lda _16bit_upperArgument(src1)
			sbc #0
			sta _16bit_upperArgument(dst)
	}
}

.pseudocommand sub16_8 src1 ; src2 ; dst {
	sec
	:sbc16_8 src1 ; src2 ; dst
}

.pseudocommand mul16 src1 ; src2 ; dst ; hop {
		// src1 will be destroyed
		:clr16 dst
		:_load hop ; #16
	loop:
		:asl16 dst
		:asl16 src1
		bcc next
		:add16 dst ; src2
	next:
		:dec hop
		bpl loop
}

.pseudocommand mul16_8 src1 ; src2 ; dst {
	lda #$00
	tay
	beq enterLoop

doAdd:
	clc
	adc src1.getValue()+0
	tax

	tya
	adc src1.getValue()+1
	tay
	txa

loop:
	asl src1.getValue()+0
	rol src1.getValue()+1
enterLoop:
	lsr src2
	bcs doAdd
	bne loop

	sta dst.getValue()+0
	sty dst.getValue()+1
}

.pseudocommand div16_rem_is_zero src1 ; src2 ; quo ; rem ; hop { 
		// src1 will be destroyed
		:_load hop ; #16
	loop:
		:asl16 src1
		:rol16 rem
		:sub16 rem ; src2 ; rem
		bcs next
		:add16 rem ; src2 ; rem
		clc
	next:
		:rol16 quo
		:dec hop
		bne loop
}

.pseudocommand div16 src1 ; src2 ; quo ; rem ; hop { 
		:clr16 rem
		:div16_rem_is_zero src1 ; src2 ; quo ; rem ; hop
}

.pseudocommand div16_round src1 ; src2 ; quo ; rem ; hop { 
	:div16 src1 ; src2 ; quo ; rem ; hop
	:asl16 rem
	:if16 rem ; GE ; src2 ; ENDIF ; !endif+
		:inc16 quo
	!endif:
}

.pseudocommand if16 not ; arg1 ; cmpr ; arg2 ; skip ; mode ; pc ; hop {
	.if(_isNOT(not)){
		.eval not = true
	}else{
		.eval hop = pc
		.eval pc = mode
		.eval mode = skip
		.eval skip = arg2
		.eval arg2 = cmpr
		.eval cmpr = arg1
		.eval arg1 = not
		.eval not = false
	}
	.if(_isSKIP(skip)){
		.eval skip = true
	}else{
		.eval hop = pc
		.eval pc = mode
		.eval mode = skip
		.eval skip = false
	}
	
	.if(!_isjump(mode)){
		.eval hop = pc
		.eval pc = mode
		.eval mode = BRANCH
	}
	
	.if(not){
		.if(_equalcomparators(cmpr, EQ)){
			.eval cmpr = NE
		}else .if(_equalcomparators(cmpr, NE)){
			.eval cmpr = EQ
		}else .if(_equalcomparators(cmpr, LT)){
			.eval cmpr = GE
		}else .if(_equalcomparators(cmpr, LE)){
			.eval cmpr = GT
		}else .if(_equalcomparators(cmpr, GE)){
			.eval cmpr = LT
		}else .if(_equalcomparators(cmpr, GT)){
			.eval cmpr = LE
		}else .if(_equalcomparators(cmpr, Lx)){
			.eval cmpr = Gx
		}else .if(_equalcomparators(cmpr, Gx)){
			.eval cmpr = Lx
		}
	}
	
	.if(_equalcomparators(cmpr, EQ) || _equalcomparators(cmpr, NE)){
	
		.if(!skip){
			.if(_isBRANCH(mode)){
				:if NOT ; _16bit_upperArgument(arg1) ; cmpr ; _16bit_upperArgument(arg2) ; done ; hop
				:if       _16bit_lowerArgument(arg1) ; cmpr ; _16bit_lowerArgument(arg2) ; pc ; hop
			}else{
				:if NOT ; _16bit_upperArgument(arg1) ; cmpr ; _16bit_upperArgument(arg2) ; done ; hop
				:if NOT ; _16bit_lowerArgument(arg1) ; cmpr ; _16bit_lowerArgument(arg2) ; done ; hop
				.if(_isJMP(mode)){
					jmp pc
				}else{
					jsr pc
				}
			}
		}else{
			.if(_isBRANCH(mode)){
				:if NOT ; _16bit_upperArgument(arg1) ; cmpr ; _16bit_upperArgument(arg2) ; pc ; hop
				:if NOT ; _16bit_lowerArgument(arg1) ; cmpr ; _16bit_lowerArgument(arg2) ; pc ; hop
			}else{
				:if       _16bit_upperArgument(arg1) ; cmpr ; _16bit_upperArgument(arg2) ; check2 ; hop
				.if(_isJMP(mode)){
					jmp pc
				}else{
					jsr pc
				}
			check2:
				:if       _16bit_lowerArgument(arg1) ; cmpr ; _16bit_lowerArgument(arg2) ; done ; hop
				.if(_isJMP(mode)){
					jmp pc
				}else{
					jsr pc
				}
			}
		}
		done:
	}else{
	
		.if(_equalcomparators(cmpr, LT) || _equalcomparators(cmpr, LE) || _equalcomparators(cmpr, Lx)){
			.var pos = LT
			.var neg = GT
		}else .if(_equalcomparators(cmpr, GT) || _equalcomparators(cmpr, GE) || _equalcomparators(cmpr, Gx)){
			.var pos = GT
			.var neg = LT
		}else{
			.error "if16: unknown comparator"
		}
		
		// all text's refer to less... greater will also handled
		.if(!skip){
			.if(_isBRANCH(mode)){
				// check weather upper is less = early done
				:if       _16bit_upperArgument(arg1) ;  pos ; _16bit_upperArgument(arg2) ; pc ; hop

				// check weather upper is bigger = early fail
				:if       _16bit_upperArgument(arg1) ;  neg ; _16bit_upperArgument(arg2) ; done ; hop

				// check weather lower fits...
				:if       _16bit_lowerArgument(arg1) ; cmpr ; _16bit_lowerArgument(arg2) ; pc ; hop

			}else{
				// check weather upper is less = early done
				:if NOT ; _16bit_upperArgument(arg1) ;  pos ; _16bit_upperArgument(arg2) ; check2 ; hop
				.if(_isJMP(mode)){
					jmp pc
				}else{
					jsr pc
				}
			check2:

				// check weather upper is bigger = early fail
				:if       _16bit_upperArgument(arg1) ;  neg ; _16bit_upperArgument(arg2) ; done ; hop

				// check weather lower fits...
				:if NOT ; _16bit_lowerArgument(arg1) ; cmpr ; _16bit_lowerArgument(arg2) ; done ; hop
				.if(_isJMP(mode)){
					jmp pc
				}else{
					jsr pc
				}
			}
		}else{ // skip mode (aka ELSE, ENDIF)
			.if(_isBRANCH(mode)){
				// check weather upper is less = early done
				:if       _16bit_upperArgument(arg1) ;  pos ; _16bit_upperArgument(arg2) ; done ; hop

				// check weather upper is bigger = early fail
				:if       _16bit_upperArgument(arg1) ;  neg ; _16bit_upperArgument(arg2) ; pc ; hop

				// check weather lower fits...
				:if NOT ; _16bit_lowerArgument(arg1) ; cmpr ; _16bit_lowerArgument(arg2) ; pc ; hop

			}else{
				.error "skip + jump on L? and G? is currently not supported"
				// check weather upper is less = early done
				:if NOT ; _16bit_upperArgument(arg1) ;  pos ; _16bit_upperArgument(arg2) ; check2 ; hop
				.if(_isJMP(mode)){
					jmp pc
				}else{
					jsr pc
				}
			check2:

				// check weather upper is bigger = early fail
				:if       _16bit_upperArgument(arg1) ;  neg ; _16bit_upperArgument(arg2) ; done ; hop

				// check weather lower fits...
				:if NOT ; _16bit_lowerArgument(arg1) ; cmpr ; _16bit_lowerArgument(arg2) ; done ; hop
				.if(_isJMP(mode)){
					jmp pc
				}else{
					jsr pc
				}
			}
		}
		done:
	}
	
}
