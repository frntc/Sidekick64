# Sidekick64

Sidekick64 is a versatile cartridge for the C64 and C128 whose functionality is defined by software running on a Raspberry Pi 3 (RPi). The connecting circuitry is quite simple and does not include any programmable ICs. Example use cases are connecting the RPi to the expansion port where it can emulate a GeoRAM/NeoRAM-compatible memory expansion, a (freezer or Easyflash) cartridge, or run a Dual-SID plus FM emulation (the SID emulation would also work when connecting the RPi to the SID-socket on the board). But many more things are imaginable, e.g. 80 column cards with HDMI video output, custom accelerators/coprocessors etc. 

Sidekick64 is a result, or cumulation, of the RasPIC64 project, which is the framework enabling a RPi to bidirectionally communicate on the bus of a Commodore 64/128. Currently it is set up to work with a Raspberry Pi 3A+ or 3B+.

<img src="Interface/sidekick64_rpi3a.jpg" height="150">  <img src="Interface/sidekick64_mainmenu.jpg" height="150">  <img src="Interface/sidekick64_config.jpg" height="150">  <img src="Interface/sidekick64_browser.jpg" height="150"> 
  
## How does it work?

On the hardware side connecting the C64 to the RPi requires level shifting to interface the 5V bus with the 3.3V GPIOs of the RPi. However, things get a bit more complicated: communication on the data lines needs to be bidirectional, the RPi needs to get hands off the bus when it's not its turn. And even worse, there are too few GPIOs on a standard RPi 3B/3B+ to simply connect to all signals! For the aforementioned use cases where we need to read address lines A0-A12, IO1, IO2, ROML, ROMH, Phi2, Reset, SID-chipselect, R/W and read/write data lines D0-D7 (plus GPIOs for controlling the circuitry). This makes the use of multiplexers necessary. Additionally we would also need to control GAME, EXROM, NMI, DMA and RESET, and, very important :-), drive a tiny OLED display and LEDs.

On the software side, handling the communication is time critical. The communication needs to happen within a time window of approx. 500 nanoseconds. This includes reading the GPIOs, switching multiplexers and reading again, possibly figuring out which data to put on the bus, doing so -- and getting hands off again on time. Many of these steps obviously must not happen too late, but also not too early (e.g. some signals might not be ready). Just sniffing the bus, e.g. getting SID commands and running an emulation is less time critical than reacting, for instance when emulating a cartridge. In this case one has to put the next byte on the bus and if too late, the C64 will most likely crash. That is, in addition to careful timing – and this turned out to be the most complicated part on the RPi – we also must avoid cache misses. 

I implemented the communication in a fast interrupt handler (FIQ) which handles GPIO reading, preloading caches if required, and reacts properly. The timing is done using the ARM's instruction counters. I use Circle (https://github.com/rsta2/circle) for bare metal programming the RPi which provides a great basis. Unfortunately other features which require IRQs (e.g. USB) interfere with the FIQ and do not work (yet?). 

## What’s in the repository
In the repo you’ll find Gerber files to produce the Sidekick64-PCB, which plugs to the C64’s expansion port on one side, and to the Raspberry Pi 3B+ (currently the full set of features is set up for the 3B+-model only) on the other one.
It also contains various smaller example programs, I recommend looking at kernel_cart and kernel_georam first – and also the Sidekick64 software which glues together various functionalities (RAM expansion, sound emulation, kernal replacement, Easyflash/MagicDesk .CRT, and PRG launching). This also requires some code compiled to run on the C64. For convenience, I added a complete image which you can copy to an SD-card and get started right away.


## Setting up the hardware

Step 1 is obviously building the PCB, it only uses simple (SMD) components:

| IC,C,R   |      value |
|----------|:-------------|
| U1 		| 74LVX573 Package_SO:SO-20_12.8x7.5mm_P1.27mm | 
| U2,U3 		| 74LVC245 Package_SO:SO-20_12.8x7.5mm_P1.27mm| 
| U5, U7, U8 	| 74LVC257 Package_SO:SSOP-16_4.4x5.2mm_P0.65mm| 
| U9 		| 74HCT30 Package_SO:SOIC-14_3.9x8.7mm_P1.27mm| 
| U6 		| 74LVC07 Package_SO:SOIC-14_3.9x8.7mm_P1.27mm| 
| D1-D4 		| LEDs (the side closer to the push bottons is GND)| 
| R1-R4 		| 0805 resistors for LEDs (I used 1.8k)| 
| R5, R6 		| 0805 10k-20k pullups| 
| C10 		| 10uF | 
| C11, C12, C13 	| not required (leave out)| 
| other caps	| 0805 100nF| 
| J5		| connection to HIRAM-pin of the CPU, close jumper if you don't connect| 
| J10		| connection to SID chip select, close jumper if you don't connect| 

You also need some pinheaders (obviously), a 2x20 female pin header, and buttons. And optionally (but recommended) a SSD1306 OLED (4 pin connector).

d=0..3 is an external delay of the multiplexer switching signal. Right now you only need to connect the center one to d=0.



## Quick start

The easiest way (and probably the one most will use) is to copy the image onto an SD card. It contains the main-Sidekick64 software combining various functionality accessibly from a menu. You can configure this menu by editing SD:C64/sidekick64.cfg and copying your Easyflash/MagicDesk/CBM80 .CRTs (others not supported), .PRG, .D64s, Final Cartridge 3/Action Replay >4.x CRTs and Kernal ROMs (.bin raw format) to the respective subdirectories.

From the menu you can select/browse (should be self-explanatory), please RESET for 1-2 seconds to get back to the main menu from other functionality. Sometimes “button 2” has extra function (e.g. freezing, reactivating Sideback after exit to basic, toggle LEDs in sound emulation, ...). You can also combine Geo/NeoRAM with Kernal-replacement and then launch a program, or use sound emulation (currently: analog output) with the .PRG launcher.

IMPORTANT: when using a SID (or replacement) which supports register reading then run the SID+FM emulation only with “register read” in the settings menu turned OFF. Otherwise the RPi and the SID may write to the bus at the same time, and this may not be a good idea.

## Known limitations/bugs

Please keep in mind that you're not reading about a product, but my personal playground. Not all kinds of .CRTs are supported (in fact only generic carts, Easyflash (without EAPI), Magic Desk, Final Cartidge 3 and Action Replay). Not all kinds of disk images are supported (I only tried D64, D71 might work, D81 not). At (hopefully rare occasions) there might be some glitches due to cache misses, e.g. a EF-CRT might not start on the first try, or the FC3 freezer might crash (press reset and it should be all fine afterwards).

## Building the code (if you want to...)

Setup your Circle40+ and gcc-arm environment, then you can compile Sidekick64 almost like any other example program (the repository contains the build settings for Circle that I use -- make sure you use them, otherwise it will probably not work). Use "make -kernel={sid|cart|ram|ef|menu}" to build the different kernels, then put the kernel together with the Raspberry Pi firmware on an SD(HC) card with FAT file system and boot your RPi with it (the “menu”-kernel is the aforementioned main software). Although the circuitry has pull-ups/pull-downs to not mess with the bus at boot time, I recommend to boot the RPi first and then turn on the C64. The RPi is ready when the splash screen appears.

The C64 code is compiled using cc65 and 64tass.

## License

My portions of the source code are work licensed under GPLv3.

The PCB is work licensed under a Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License.

## Disclaimer

Be careful not to damage your RPi or C64, or anything attached to it. I am not responsible if you or your hardware is damaged. In principle, you can attach the RPi and other cartridges at the same time, as long as they do not conflict (e.g. in IO ranges or driving signals). I recommend to NOT do this. For example, using Sidekick and an Easyflash 3 will very likely destroy the EF3's CPLD. Again, I'm not taking any responsibility -- if you don't know what you're doing, better don't... use everything at your own risk.

## Misc 

Last but not least I would like to thank a few people and give proper credits:

kinzi (forum64.de, F64) for lots of discussions and explanations on electronics and how a C64 actually works, Kim Jørgensen for chats on weird bus timings and freezers, and hints on how to get things right, and the testers on F64 (bigby, emulaThor, kinzi).
Rene Stange (the author of Circle) for his framework and patiently answering questions on it, and digging into special functionality (e.g. ARM stubs without L1 prefetching). Retrofan (https://compidiaries.wordpress.com/) for sharing his new system font which is also used in this release.
The authors of reSID and the OPL emulators (also used in WinVICE), the authors of SSD1306xLED  (https://bitbucket.org/tinusaur/ssd1306xled, which I modified to work in this setting) for making their work available. The code in the repo further builds on d642prg V2.09 and some other code fragments found on cbm-hackers and codebase64.org. The OLED-Sidekick logo is based on a font by Atomic Flash found at https://codepo8.github.io/logo-o-matic.


### Trademarks

Raspberry Pi is a trademark of the Raspberry Pi Foundation.
