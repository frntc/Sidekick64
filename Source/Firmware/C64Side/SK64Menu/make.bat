cls
del cart64_16k.o rpimenu_sub.o main.o test.bin crt0.o main.s 

rem patching c64.lib
xcopy /Y c64.lib c64-cart.lib
..\..\bin\ar65 d c64-cart.lib crt0.o
..\..\bin\ca65 cart64_16k.S
..\..\bin\ar65 a c64-cart.lib cart64_16k.o

..\..\bin\cc65 -t c64 -Cl -T main.c 
..\..\bin\ca65 -t c64 main.s
..\..\bin\ca65 -t c64 rpimenu_sub.s
..\..\bin\ld65 -v -vm -m cart.map --config cart64_16k.cfg -o test.bin main.o rpimenu_sub.o cart64_16k.o  c64-cart.lib

rem z: is the Sidekick-SD-card
xcopy /Y test.bin z:\C64\menu.bin
