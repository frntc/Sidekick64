/*
** basic loader
*/

.macro BasicLoader(start){
	.pc = $0801 "Basic Loader"
		.word nextline
		.word 3880
		.byte $9e
		.text toIntString(start)
		.byte 0
	nextline:
		.word 0
}

/*
** read a full charset
*/

.macro CharSet(file){
	.var charset = LoadPicture(file, List().add($ffffff, $000000)) 
	.for (var y=0; y<8; y++) 
		.for (var x=0;x<32; x++) 
			.for(var charPosY=0; charPosY<8; charPosY++) 
				.byte charset.getSinglecolorByte(x,charPosY+y*8) 
}


/*
** Overall Memorymap
*/

/*
.pc = $0000 "Zero Page" virtual
.fill $100, 0
.pc = $0100 "Stack" virtual
.fill $100, 0
.pc = $0200 "Kernal/Basic Temp" virtual
.fill $200, 0
.pc = $a000 "BASIC ROM" virtual
.fill $2000, 0
.pc = $d000 "I/O or Char Rom" virtual
.fill $1000, 0
.pc = $e000 "KERNAL ROM" virtual
.fill $2000, 0
.pc = $d800 "Color Memory" virtual
.fill $400, 0
*/

/*
** load indirect
*/

.macro lday(zp, ofs) {
	ldy #ofs
	lda (zp), y
}

.macro stay(zp, ofs) {
	ldy #ofs
	sta (zp), y
}

.macro movy(zp, ofs, dst) {
	ldy #ofs
	lda (zp), y
	sta dst
}

/*
** Constants
*/

// VIC
.const BORDER_COLOR = $d020
.const BACKGROUND_COLOR = $d021
.const COLOR_MEMORY = $d800

// KERNAL
.const READST = $ffb7
.const SETLFS = $ffba
.const SETNAM = $ffbd
.const OPEN = $ffc0
.const CLOSE = $ffc3
.const CHKIN = $ffc6
.const CHKOUT = $ffc9
.const CHRIN = $ffcf
.const CHROUT = $ffd2
.const GETIN = $ffe4

.const ZP_LAST_USED_DRIVE = $ba

/*
** en-/disable kernal
*/

.macro UseHidden(){
		sei
		:mov #$31 ; $01 // only I/O
}

.macro UseKernal(){
		:mov #$36 ; $01 // only I/O + KERNAL
		cli
}

.macro CallHidden(func){
		:UseHidden()
		jsr func
		:UseKernal()
}
