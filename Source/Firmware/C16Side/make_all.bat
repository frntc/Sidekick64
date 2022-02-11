..\bin\cc65 -t c16 -T -O --static-locals rpimenu16.c
..\bin\ca65 -t c16 rpimenu16_sub.s
..\bin\ca65 -t c16 rpimenu16.s
..\bin\ld65 -C ..\cfg\c16.cfg -o rpimenu16.prg rpimenu16.o rpimenu16_sub.o -L ..\lib --lib c16.lib

64tass cart264.a --nostart --output=launch16.cbm80

 ..\bin\cc65 -t c16 -T -O --static-locals neoramc16.c
 ..\bin\ca65 -t c16 neoramc16.s
 ..\bin\ld65 -C ..\cfg\c16.cfg -o neoramc16.prg neoramc16.o -L ..\lib --lib c16.lib

64tass --ascii sid.a --output=eternity2sid.prg

64tass --ascii edlib.a --output=edlib264.prg

