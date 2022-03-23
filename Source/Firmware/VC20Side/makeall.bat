64tass cartvic20.a --nostart
xcopy /Y a.out z:\VC20\launch20.a0cbm

64tass cartvic20_fast.a --nostart
xcopy /Y a.out z:\VC20\launch20_35k.a0cbm

64tass vic20_crt_sync.a --nostart
xcopy /Y a.out z:\VC20\crtsync20.a0cbm

64tass vic20_basic_sync.a --nostart
xcopy /Y a.out z:\VC20\basicsync20.a0cbm

64tass --nostart wait4NMI_VC20.a
xcopy /y a.out waitnmi
bin2c -o waitnmi.h waitnmi
cc65 -t vic20 -T -O --static-locals vc20.c
ca65 -t vic20 vc20.s
ld65 -C vic20-32k_mine.cfg --mapfile vc20.map -o vc20.prg vc20.o -L ..\lib --lib vic20.lib
xcopy /Y vc20.prg z:\VC20\rpimenu20.prg
