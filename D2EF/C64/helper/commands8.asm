/*
** 8bit commands
*/

.pseudocommand clr dst ; hop { 
	.if(_isregister8(dst)){
		:load dst ; #0
	}else{
		.if(_isunset(hop)){
			.eval hop = A
		}else .if(! _isregister8(hop)){
			.error "clr: hop is not a register"
		}
		:_load hop ; #0
		:_store hop ; tar
	}
}

.pseudocommand mov src ; hop ; dst {
	// load src in hop (default A) and store it to dst
	// hop is ignored when src and/or dst is a register
	.if(_isunset(dst)){
		.eval dst = hop
		.eval hop = _DEFAULT
	}else .if(_isunset(hop)){
		.eval hop = _DEFAULT
	}else .if(! _isregister8(hop) && !_isdefault(hop)){
		.error "mov: hop is not a register"
	}
	.if(_isregister8(src)){
		.if(! _isdefault(hop)){
			.error "mov: if src and/or dst is a register hop must not be set"
		}
		// src = REGISTER
		.if(_isregister8(dst)){
			// src && dst = REGISTER
			.if(_equalregisters8(src, dst)){
				// same register: done
			}else .if(_equalregisters8(src, A) && _equalregisters8(dst, X)){
				tax
			}else .if(_equalregisters8(src, A) && _equalregisters8(dst, Y)){
				tay
			}else .if(_equalregisters8(src, X) && _equalregisters8(dst, A)){
				txa
			}else .if(_equalregisters8(src, Y) && _equalregisters8(dst, A)){
				tya
			}else{
				.error "mov: unable to move register " + src + " directly to register " + dst
			}
		}else{
			// src = REGISTER; dst = no regsiter
			:_store src ; dst
		}
	}else .if(_isregister8(dst)){
		.if(! _isdefault(hop)){
			.error "mov: if src and/or dst is a register hop must not be set"
		}
		// src = no regsiter; dst = REGISTER
		:_load dst ; src
	}else{
		.if(_isdefault(hop)){
			.eval hop = A
		}
		:_load hop ; src
		:_store hop ; dst
	}
}

.pseudocommand inc dst {
	.if(_equalregisters8(dst, A)){
		clc
		adc #1
	}else .if(_equalregisters8(dst, X)){
		inx
	}else .if(_equalregisters8(dst, Y)){
		iny
	}else{
		inc dst
	}
}

.pseudocommand dec dst {
	.if(_equalregisters8(dst, A)){
		sec
		sbc #1
	}else .if(_equalregisters8(dst, X)){
		dex
	}else .if(_equalregisters8(dst, Y)){
		dey
	}else{
		dec dst
	}
}

.pseudocommand adc src1 ; src2 ; dst {
	.if(! _equalregisters8(src1, A)){
		lda src1
	}
		
	adc src2 
	
	.if(_isunset(dst)){
		.eval dst = src1
	}

	.if(! _equalregisters8(dst, A)){
		sta dst
	}
}

.pseudocommand add src1 ; src2 ; dst { 
	clc
	:adc src1 ; src2 ; dst
}

.pseudocommand sbc src1 ; src2 ; dst {
	.if(! _equalregisters8(src1, A)){
		lda src1
	}
		
	sbc src2 
	
	.if(_isunset(dst)){
		.eval dst = src1
	}

	.if(! _equalregisters8(dst, A)){
		sta dst
	}
}

.pseudocommand sub src1 ; src2 ; dst { 
	sec
	:sbc src1 ; src2 ; dst
}

.pseudocommand mul8_16 src1 ; src2 ; dst ; hop {
		:_load hop ; #8
		:clr16 dst
	loop:
		:asl16 dst
		asl src1
		bcc next
		lda dst
		clc
		adc src2
		sta _16bit_lowerArgument(dst)
		bcc next
		inc _16bit_upperArgument(dst)
	next:
		:dec hop
		bne loop
}

.pseudocommand mul10 src ; tar ; buffer {
	.if(!_equalregisters8(src, A)){
		lda arg1
	}
	.if(_isSMC(buffer)){
		// use self-modifying code
		asl
		sta two+1
		asl
		asl
		clc
		two: adc #0
	}else{
		// use a buffer anywhere
		asl
		sta buffer
		asl
		asl
		clc
		adc buffer
	}
	.if(!_equalregisters8(tar, A)){
		sta tar
	}
}

.pseudocommand if not ; arg1 ; cmpr ; arg2 ; skip ; mode ; pc ; hop {
	.if(_isNOT(not)){
		.eval not = true
	}else{
		.eval pc = mode
		.eval mode = skip
		.eval skip = arg2
		.eval arg2 = cmpr
		.eval cmpr = arg1
		.eval arg1 = not
		.eval not = false
	}
	.if(_isSKIP(skip)){
		.eval not = ! not
	}else .if(!_isunset(skip)){
		.eval hop = pc
		.eval pc = mode
		.eval mode = skip
		//.eval skip = ... is not used anymore
	}
	.if(_isunset(pc) || _isregister8(pc)){
		.eval hop = pc
		.eval pc = mode
		.eval mode = BRANCH
	}
	.if(_isunset(hop)){
		.eval hop = _DEFAULT
	}else .if(! _isregister8(hop)){
		.error "if: hop is not a register"
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
	
	.if(_equalcomparators(cmpr, EQ)){
		:_eq hop ; arg1 ; arg2 ; mode ; pc
	}else .if(_equalcomparators(cmpr, NE)){
		:_ne hop ; arg1 ; arg2 ; mode ; pc
	}else .if(_equalcomparators(cmpr, LT) || _equalcomparators(cmpr, Lx)){
		:_lt hop ; arg1 ; arg2 ; mode ; pc
	}else .if(_equalcomparators(cmpr, LE)){
		:_le hop ; arg1 ; arg2 ; mode ; pc
	}else .if(_equalcomparators(cmpr, GE) || _equalcomparators(cmpr, Gx)){
		:_ge hop ; arg1 ; arg2 ; mode ; pc
	}else .if(_equalcomparators(cmpr, GT)){
		:_gt hop ; arg1 ; arg2 ; mode ; pc
	}else{
		.error "if: unknown comparator"
	}
	
}
