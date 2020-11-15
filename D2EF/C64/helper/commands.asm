/*
**
**  Please read the manual!
**
**
**
**
**  This file do contains only the constants and all the internal stuff...
**  All internal functions have to be called with all arguments set.
**  hop is always either _DEFAULT or a register
*/


/*
** registers & other tags
*/

.var _noneid = 1

.enum {
	// default (for hop)
	_DEFAULT = CmdArgument(AT_NONE, _noneid++),
	
	// 8bit registers
	A = CmdArgument(AT_NONE, _noneid++),
	X = CmdArgument(AT_NONE, _noneid++),
	Y = CmdArgument(AT_NONE, _noneid++),
	
	// 16bit registers
	XY = CmdArgument(AT_NONE, _noneid++),
	
	// self modifying code
	SMC = CmdArgument(AT_NONE, _noneid++),

	// mode:J	
	JMP = CmdArgument(AT_NONE, _noneid++),
	JSR = CmdArgument(AT_NONE, _noneid++),
	BRANCH = CmdArgument(AT_NONE, _noneid++),
	
	// comparators
	EQ = CmdArgument(AT_NONE, _noneid++),
	NE = CmdArgument(AT_NONE, _noneid++),
	LT = CmdArgument(AT_NONE, _noneid++),
	LE = CmdArgument(AT_NONE, _noneid++),
	GE = CmdArgument(AT_NONE, _noneid++),
	GT = CmdArgument(AT_NONE, _noneid++),
	Lx = CmdArgument(AT_NONE, _noneid++),
	Gx = CmdArgument(AT_NONE, _noneid++),
	
	// not / skip
	NOT = CmdArgument(AT_NONE, _noneid++),
	ELSE = CmdArgument(AT_NONE, _noneid),  // dont inc, it's same as SKIP
	ENDIF = CmdArgument(AT_NONE, _noneid),  // dont inc, it's same as SKIP
	SKIP = CmdArgument(AT_NONE, _noneid++)
}

/*
** special tags
*/

.function _isunset(r){
	.return r.getType() == AT_NONE && r.getValue() == 0
}

.function _isdefault(r){
	.return r.getType() == AT_NONE && r.getValue() == _DEFAULT.getValue()
}

.function _isSMC(r){
	.return r.getType() == AT_NONE && r.getValue() == SMC.getValue()
}

.function _isBRANCH(r){
	.return r.getType() == AT_NONE && r.getValue() == BRANCH.getValue()
}

.function _isJMP(r){
	.return r.getType() == AT_NONE && r.getValue() == JMP.getValue()
}

.function _isJSR(r){
	.return r.getType() == AT_NONE && r.getValue() == JSR.getValue()
}

.function _isjump(r){
	.return r.getType() == AT_NONE && r.getValue() >= JMP.getValue() && r.getValue() <= BRANCH.getValue()
}

.function _isNOT(r){
	.return r.getType() == AT_NONE && r.getValue() == NOT.getValue()
}

.function _isSKIP(r){
	.return r.getType() == AT_NONE && r.getValue() == SKIP.getValue()
}

/*
** 8bit
*/

.function _isregister8(r){
	.return r.getType() == AT_NONE && r.getValue() >= A.getValue() && r.getValue() <= Y.getValue()
}

.function _equalregisters8(r1, r2){
	.return _isregister8(r1) && _isregister8(r2) && r1.getValue() == r2.getValue()
}

.pseudocommand _load reg ; arg {
	.if(_equalregisters8(reg, A)){
		lda arg
	}else .if(_equalregisters8(reg, X)){
		ldx arg
	}else .if(_equalregisters8(reg, Y)){
		ldy arg
	}else{
		.error "can only load memory into the registers A,X,Y"
	}
}

.pseudocommand _store reg ; arg {
	.if(_equalregisters8(reg, A)){
		sta arg
	}else .if(_equalregisters8(reg, X)){
		stx arg
	}else .if(_equalregisters8(reg, Y)){
		sty arg
	}else{
		.error "can only store the registers A,X,Y into memory"
	}
}

.pseudocommand _compare reg ; arg {
	.if(_equalregisters8(reg, A)){
		cmp arg
	}else .if(_equalregisters8(reg, X)){
		cpx arg
	}else .if(_equalregisters8(reg, Y)){
		cpy arg
	}else{
		.error "can only compare registers A,X,Y with memory"
	}
}

/*
** 16bit
*/

.function _isregister16(r){
	.return r.getType() == AT_NONE && r.getValue() >= XY.getValue() && r.getValue() <= XY.getValue()
}

.function _equalregisters16(r1, r2){
	.return _isregister16(r1) && _isregister16(r2) && r1.getValue() == r2.getValue()
}

.function _16bit_lowerArgument(arg) { 
	.if (arg.getType() == AT_IMMEDIATE){
		.return CmdArgument(arg.getType(), <arg.getValue())
	}else .if(_equalregisters16(arg, XY)){
		.return X
	}else{
		.return arg
	}
} 

.function _16bit_upperArgument(arg) { 
	.if (arg.getType() == AT_IMMEDIATE){
		.return CmdArgument(arg.getType(), >arg.getValue())
	}else .if(_equalregisters16(arg, XY)){
		.return Y
	}else{
		.return CmdArgument(arg.getType(), arg.getValue()+1)
	}
} 

/*
** 24bit
*/

.function _24bit_lowerArgument(arg) { 
		.return arg
} 

.function _24bit_middleArgument(arg) { 
		.return CmdArgument(arg.getType(), arg.getValue()+1)
} 

.function _24bit_upperArgument(arg) { 
		.return CmdArgument(arg.getType(), arg.getValue()+2)
} 

/*
** branching
*/

.pseudocommand _beq mode ; pc {
	.if(_isBRANCH(mode)){
			beq pc
	}else .if(_isJMP(mode)){
			bne skip
			jmp pc
		skip:
	}else .if(_isJSR(mode)){
			bne skip
			jsr pc
		skip:
	}else{
		.error "unknown branch/jump mode"
	}
}

.pseudocommand _bne mode ; pc {
	.if(_isBRANCH(mode)){
			bne pc
	}else .if(_isJMP(mode)){
			beq skip
			jmp pc
		skip:
	}else .if(_isJSR(mode)){
			beq skip
			jsr pc
		skip:
	}else{
		.error "unknown branch/jump mode"
	}
}

.pseudocommand _bcc mode ; pc {
	.if(_isBRANCH(mode)){
			bcc pc
	}else .if(_isJMP(mode)){
			bcs skip
			jmp pc
		skip:
	}else .if(_isJSR(mode)){
			bcs skip
			jsr pc
		skip:
	}else{
		.error "unknown branch/jump mode"
	}
}

.pseudocommand _bcs mode ; pc {
	.if(_isBRANCH(mode)){
			bcs pc
	}else .if(_isJMP(mode)){
			bcc skip
			jmp pc
		skip:
	}else .if(_isJSR(mode)){
			bcc skip
			jsr pc
		skip:
	}else{
		.error "unknown branch/jump mode"
	}
}

/*
** compare
*/

.function _iscomparator(r){
	.return r.getType() == AT_NONE && r.getValue() >= EQ.getValue() && r.getValue() <= Gx.getValue()
}

.function _equalcomparators(c1, c2){
	.return _iscomparator(c1) && _iscomparator(c2) && c1.getValue() == c2.getValue()
}

.pseudocommand _eq hop ; arg1 ; arg2 ; mode ; pc {
	.if(_isregister8(arg1)){
		.if(!_isdefault(hop)){
			.error "if: if at least one of the args is a register, hop must be not set"
		}
		:_compare arg1 ; arg2
	}else .if(_isregister8(arg2)){
		.if(!_isdefault(hop)){
			.error "if: if at least one of the args is a register, hop must be not set"
		}
		:_compare arg2 ; arg1
	}else{
		.if(_isdefault(hop)){
			.eval hop = A
		}
		:_load hop ; arg1
		.if(arg2.getType() != AT_IMMEDIATE || arg2.getValue() != 0){
			:_compare hop ; arg2
		}
	}
	:_beq mode ; pc
}

.pseudocommand _ne hop ; arg1 ; arg2 ; mode ; pc {
	.if(_isregister8(arg1)){
		.if(!_isdefault(hop)){
			.error "if: if at least one of the args is a register, hop must be not set"
		}
		:_compare arg1 ; arg2
	}else .if(_isregister8(arg2)){
		.if(!_isdefault(hop)){
			.error "if: if at least one of the args is a register, hop must be not set"
		}
		:_compare arg2 ; arg1
	}else{
		.if(_isdefault(hop)){
			.eval hop = A
		}
		:_load hop ; arg1
		.if(arg2.getType() != AT_IMMEDIATE || arg2.getValue() != 0){
			:_compare hop ; arg2
		}
	}
	:_bne mode ; pc
}

.pseudocommand _le hop ; arg1 ; arg2 ; mode ; pc {
	.if(_isregister8(arg1)){
		.if(!_isdefault(hop)){
			.error "if: if at least one of the args is a register, hop must be not set"
		}
		.if(arg2.getType() == AT_IMMEDIATE){
			.if(arg2.getValue() == 255){
				.error "if: R <= 255 is always true"
			}else{
				// R <= M  ->  R < M+1
				:_compare arg1 ; # arg2.getValue()+1
				:_bcc mode ; pc
			}
		}else{
			:_compare arg1 ; arg2
			:_beq mode ; pc
			:_bcc mode ; pc
		}
	}else .if(_isregister8(arg2)){
		.if(!_isdefault(hop)){
			.error "if: if at least one of the args is a register, hop must be not set"
		}
		// M <= R   ->   R >= M
		:_compare arg2 ; arg1
		:_bcs mode ; pc
	}else{
		.if(_isdefault(hop)){
			.eval hop = A
		}
		// M <= N   ->   N >= M
		:_load hop ; arg2
		:_compare hop ; arg1
		:_bcs mode ; pc
	}
}

.pseudocommand _lt hop ; arg1 ; arg2 ; mode ; pc {
	.if(_isregister8(arg1)){
		.if(!_isdefault(hop)){
			.error "if: if at least one of the args is a register, hop must be not set"
		}
		// R < M
		:_compare arg1 ; arg2
		:_bcc mode ; pc
	}else .if(_isregister8(arg2)){
		.if(!_isdefault(hop)){
			.error "if: if at least one of the args is a register, hop must be not set"
		}
		// M < R  ->  R > M
		:_gt arg2 ; arg1 ; pc
	}else{
		.if(_isdefault(hop)){
			.eval hop = A
		}
		// M < N
		:_load hop ; arg1
		:_compare hop ; arg2
		:_bcc mode ; pc
	}
}

.pseudocommand _ge hop ; arg1 ; arg2 ; mode ; pc {
	.if(_isregister8(arg1)){
		.if(!_isdefault(hop)){
			.error "if: if at least one of the args is a register, hop must be not set"
		}
		// R >= M
		:_compare arg1 ; arg2
		:_bcs mode ; pc
	}else .if(_isregister8(arg2)){
		.if(!_isdefault(hop)){
			.error "if: if at least one of the args is a register, hop must be not set"
		}
		// M >= R  ->  R <= M
		:_le arg2 ; arg1 ; pc
	}else{
		.if(_isdefault(hop)){
			.eval hop = A
		}
		// M >= N
		:_load hop ; arg1
		:_compare hop ; arg2
		:_bcs mode ; pc
	}
}

.pseudocommand _gt hop ; arg1 ; arg2 ; mode ; pc {
	.if(_isregister8(arg1)){
		.if(!_isdefault(hop)){
			.error "if: if at least one of the args is a register, hop must be not set"
		}
		.if(arg2.getType() == AT_IMMEDIATE){
			.if(arg2.getValue() == 255){
				.error "if: R > 255 is always false"
			}else{
				// R > M  ->  R >= M+1
				:_compare arg1 ; # arg2.getValue()+1
				:_bcs mode ; pc
			}
		}else{
				:_compare arg1 ; arg2
				beq skip
				:_bcs mode ; pc
			skip:
		}
	}else .if(_isregister8(arg2)){
		.if(!_isdefault(hop)){
			.error "if: if at least one of the args is a register, hop must be not set"
		}
		// M > R   ->   R < M
		:_compare arg2 ; arg1
		:_bcc mode ; pc
	}else{
		.if(_isdefault(hop)){
			.eval hop = A
		}
		// M > N   ->   N < M
		:_load hop ; arg2
		:_compare hop ; arg1
		:_bcc mode ; pc
	}
}


//import source "commands8.asm"
//import source "commands16.asm"
