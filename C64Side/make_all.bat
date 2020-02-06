64tass --nostart cart.a --output=launch.cbm80
64tass --nostart cart_ultimax.a --output=launch_ultimax.cbm80

..\bin\cc65 -t c64 -T -O --static-locals rpimenu.c
..\bin\ca65 -t c64 rpimenu_sub.s
..\bin\ca65 -t c64 rpimenu.s
..\bin\ld65 -C ..\cfg\c64.cfg -o rpimenu.prg rpimenu.o rpimenu_sub.o -L ..\lib --lib c64.lib


