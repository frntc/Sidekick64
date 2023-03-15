


<img align="right"  width="280" src="https://raw.githubusercontent.com/frntc/Sidekick64/master/Images/sidekick64_logo.jpg">


**Sidekick64** is a versatile cartridge/expansion for the C64, C128, the C16/+4, and the VIC20. It uses a Raspberry Pi 3A+, 3B+ or Zero 2 (only for C64/C128/VIC20) to emulate memory expansions, sound devices (up to 8 SIDs, SFX Sound Expander, MIDI), freezer cartridges, cartridges such as Easyflash/GMOD, kernal replacements, C128 function ROMs and many more. Some functionalities can be combined with others, e.g. kernal replacement plus freezers or GeoRAM expansion. The Sidekick64 handles among others PRGs, D64, SID, MOD, YM and WAV files and also integrates tools such as Disk2EasyFlash, PSID64.

<p align="center" font-size: 30px;>

<img src="https://raw.githubusercontent.com/frntc/Sidekick64/master/Images/sidekick64_menu.gif"  height="200">  
<img  src="https://raw.githubusercontent.com/frntc/Sidekick64/master/Images/sidekick64_rpi3a.jpg"  height="200">  
<img  src="https://raw.githubusercontent.com/frntc/Sidekick64/master/Images/sidekick64_cases.jpg"  height="200">  

</p>

Its functionality is entirely defined by software. The connecting circuitry is quite simple and does not include any programmable ICs and is thus easy to build. A more complete list of currently implemented emulations/features includes 

- a GeoRAM/NeoRAM-compatible memory expansion,
- freezer cartridges
	* Action Replay 4.x-7.x
	* Final Cartridge 3(+)
	* KCS Power Cartridge
	* Super Snapshot V5
	* Freeze Frame and Freeze Machine
	* Atomic/Nordic Power (experimental, at best)
- numerous (bank switching) cartridges such as
	* plain CBM80, C16/+4 and C128 cartridges
	* Easyflash (with simplified EAPI support)
	* GMod2 (including EEPROM for save games)
	* MagicDesk/Domark/HES Australia
	* Ocean Type A and B (up to 512k)
	* Dinamic
	* C64 Game System
	* Funplay/Powerplay
	* Comal80
	* Epyx Fastload
	* Warp Speed (C64 and C128)
	* Megabit ROM (C128)
	* (Super) Zaxxon
	* Prophet64
	* Simons Basic
	* RGCD and Hucky's cartridge
- C64 kernal replacement
- Function ROMs on a C128
- multiple SIDs and Sound Expander/FM emulation (up to 8 SIDs, e.g. to play [The Tuneful 8](https://csdb.dk/release/?id=182735))
- simplified Datel and Sequential MIDI interface emulation with built-in SoundFont-synthesizer (slightly modified version of [TinySoundFont](https://github.com/schellingb/TinySoundFont))
- audio player (MOD, YM and WAV files) with SID- and HDMI-output, supporting up to stereo 48kHz output on dual-SID setups (C64 and C128)
- TED-sound and Digiblaster emulation for C16/+4 (to have all sound devices on one output)

The **Sidekick20** (Sidekick64 on the VIC20):
- provides a VIC-emulation which outputs picture and sound via the Raspberry Pi's HDMI-output
- the emulated VIC incorporates the VFLI extension
- emulates a memory expansion (RAM1/2/3, BLK 1, 2, 3, 5, IO2/3)
- comes with a built-in rudimentary disk emulation (LOAD/SAVE via kernal vectors)
- runs PRGs and VIC20-cartridges stored as (multiple) PRG-files or CRT-files
- automatically determines memory settings for programs and carts (override with manual settings possible)

But many more things are imaginable, e.g. 80 column cards with HDMI video output, custom accelerators/coprocessors etc.

Sidekick64 has been tested with various PAL-machines (C64s, C128, C128D), C64 NTSC, C16/+4 (PAL, PAL-N, NTSC), VIC20 (PAL/NTSC), Ultimate64 and C64 Reloaded boards. Sidekick64 connects to the C16/+4 and the VIC20 using simple adapters.

The Sidekick64 software provides a main menu for frequently used features, programs, cartridges (either configured manually, or for the C64/C128 version created from a folder of favorites on the SD card), a configuration screen, and a file browser. The menu can be customized (entries, layout, color schemes, font, animation). It also autodetects and interoperates with a [SIDKick](https://github.com/frntc/SIDKick) if present. The C16/+4 version comes with two fabulous games ported to run directly off the emulated memory expansion: Alpharay and Pet's Rescue! Here's a [video](Video/Sidekick64_ElectricCity_by_Flex.mp4) of Sidekick64 emulating SIDs and playing [Electric City](https://csdb.dk/release/?id=189742) by Flex.

  
  


  
  
  
  

## How to build a Sidekick64:

<img align="right" src="https://raw.githubusercontent.com/frntc/Sidekick64/master/Images/sidekick64_sb.jpg"  height="200">  

This section summarizes building and setting up the hardware. If you're not into building one yourself: [Restore Store](https://restore-store.de) (not my shop) offers pre-assembled Sidekick64s at fair prices.

  

### PCBs, BOM and assembly information

  

There are two variants of the PCB: a larger one the fits the dimensions of the Raspberry Pi 3A+/3B+ (shown above) and a smaller one with tailored for the Raspberry Pi Zero 2 (image on the right). Note that all is interchangeable, i.e. you can use either PCB with any of the supported RPis.

  

Here you can find the BOM and assembly information for [Sidekick64 v0.42](https://htmlpreview.github.io/?https://github.com/frntc/Sidekick64/blob/master/Gerber/ibom_sk64.html), [Sidekick64 v0.42 Zero](https://htmlpreview.github.io/?https://github.com/frntc/Sidekick64/blob/master/Gerber/ibom_sk64zero.html). Sourcing components: you can use LVC and LVX types for the 07, 245, 257, and 573 ICs, and you can use 74LS30 or 74HCT30. The C16/+4 adapter does not require any electronic components. The VIC20 adapter uses only plain resistors and diodes.

  

### Build instructions

  

The first step when building the Sidekick64 is soldering the surface-mount components (top side only). Next solder the socket for the Raspberry Pi (2x20 female pin header), the pin headers and push buttons. Optionally you can attach a SSD1306 OLED (4 pin connector) or a ST7789 RGB-TFT with 240x240 resolution to the Sidekick64. The latter is recommended and provides much more functionality. Pay attention to configure the GND/VCC pins of the displays correctly using the solder pads.

  

**Important:** d=0..3 (see PCB) is an external delay of the multiplexer switching signal. For the C64/C128 use a jumper connecting the pins in the d=0 columns. For the C16/+4 connect the d=3 column!

**Important (for C64/C128):** if you want to use the kernal-replacement and SID-emulation functionality you need to connect the two pins on the left side of the PCB (dubbed "SID CS" and "CPU P1") using wires to the CPU (pin 28 for MOS 6510/8500 in C64, pin 29 for 8502 in C128) and the SID-socket (pin 8). Note that the Sound Expander and MIDI emulation works without the SID CS-wire.

The jumper "A13-BTN" is used select whether the Sidekick64 reads the A13-signal from the computer or the middle button, currently the latter is not used.

**Important (for VIC20):** set the external delay-jumper to d=0 and the "A13-BTN"-jumper to A13!

  

### 3D printed cases

My first attempt at designing a case for 3D printing is included in this repository. Much nicer ones for both the RPi 3B+ and the RPi Zero 2 have been created by bigby (who provided the photo shown above) and are available [here](https://www.printables.com/model/257138-sidekick64-zero-cartridge-case) and [here](https://www.thingiverse.com/hackup/designs). Jukk4 on forum64.de also designed a [case for the RPi 3A+](https://www.forum64.de/index.php?thread/87523-projektvorstellung-sidekick64/&postID=1691161#post1691161). [Here](https://www.printables.com/de/model/89901-sidekick-64-raspberry-3a-preliminary-version/related) is a 3A+ case derived from the case in this repository.

  

### I have a previous revision PCB, can I still...?

The software can be run on any of the previous PCB revisions. If you want to to use a RGB-TFT and/or the C128 function ROMs with your (very old) PCB then you can do simple modifications (soldering 3 wires to connect the two additional signals to the display and add one more input to the RPi) as shown [here](Images/sidekick64_rev03_upgrade.jpg). All revisions v0.4 and above do not need any modification.

  

## Quick start

  

The easiest way (and probably the one most will use) is to copy the image onto an SD card. It contains the main Sidekick64-software combining various functionality accessibly from a menu. You should edit the **configuration file** SD:C64/sidekick64.cfg and copying your .CRTs, .PRG, .D64s, kernal ROMs (.bin raw format) etc. to the respective subdirectories. You can choose to automatically create the main screen from files stored in SD:/FAVORITES (C64/C128). Don't forget to set the type of display used in this .cfg-file!  Pressing *=* cycles through your and some other color profiles.

You can also create **custom logos** to be used with Easyflash .CRTs and .PRGs (.raw format for the OLED, 24-Bit uncompressed .tga for the RGB-TFT), or modify the appearance on the TFT completely (see SD:SPLASH). There is also a command line tool to create **custom animations** visible in the C64/C128(VIC) menu. 

From the menu you can select/browse using the keyboard or a joystick in port 2 (should be self-explanatory), by pressing the RESET-button for 1-2 seconds you get back to the main menu from other functionalities.

Sometimes 'button #2' has extra function (e.g. freezing, reactivating Sidekick64 after exit to basic, toggle visualization in sound emulation, ...). 

**Important for C64/C128 use:** when using a SID (or replacement) which supports register reading (= essentially all but the non-Ultimate SwinSIDs) then run the SID+FM emulation only with **register read** in the settings menu turned **OFF**. Otherwise the Sidekick64 and the SID may write to the bus at the same time, and this may not be a good idea.

  

## Overclocking

  

When using the RPi Zero 2 you need to overclock the SoC to 1.2 GHz or 1.3 GHz (see SD:config.txt). The RPi 3A+/3B+ can run at the same frequencies when being used with a C64/C128 (i.e. without overclocking). For the C16/+4 the software is currently tested with the 3A+/3B+ only and also requires a little bit of overclocking. The VIC20 version requires some overclocking on the Zero 2, the 3A+/3B+ run quite relaxed. Although I never experienced any problems with it, please be aware that overclocking may void warranty.

  

## Powering the Sidekick64

Normally you would use an external power supply for the RPi. Although the circuitry has pull-ups/pull-downs to not mess with the bus at boot time, the safest way is to boot the Sidekick64 first and then turn on the C64. The Sidekick64 is ready when the splash screen appears.

  

Alternatively you can power the Sidekick64 from the **C64/C128** using the "close to power..."-jumper. The current of the RPi has been measured and stays within the specifications of the expansion port. In any case I recommend to not try powering the Sidekick64 with a standard PSU/brick of death.

 
In principle **Sidekick20** can be powered from the VIC20 expansion port (tested), but I was not able to find technical documents which reveal whether the current/power is actually within specs or not. 

Do NOT power the Sidekick64 from a **C16/+4**, always use an external power supply for these computers!

**Important:**  **NEVER** power the Sidekick from the computer and externally at the same time. NEVER!

  

## Sidekick128 for C128

  

The menu software is the very same as for Sidekick64 -- it detects automatically when running on a C128, and scans the PRG128 and CRT128 folders on the SD-card in this case. It can launch PRGs in C128 mode and emulate cartridges and external function ROM. The Easyflash emulation supports reads in 2MHz Turbo Mode on the C128.

The menu can also be displayed on the **VDC (40/80-column) screen on a C128**: press &uarr; or *shift*-&uarr; to cycle through VIC, VIC+VDC and VDC-only output. Pressing *shift* goes to 40-column-VDC (if the next mode includes VDC-output), no *shift* means 80-column output.
  

Known limitations: the C128 behaves differently in C64-Ultimax-mode (VIC-II ROMH read accesses do not properly set the RW-signal at the expansion port). This causes 10%-20% of the read accesses to fail "randomly", because the RW-signal is used to control the direction of level shifters on the Sidekick64. You can experience this as visual artifacts in old Ultimax-mode games.

  

## Sidekick264 for C16 and Plus/4

  

Sidekick264 requires a passive adapter (Gerber files and schematics are in the repo) to be put between the C16/+4 expansion port and the Sidekick64.

To enable Sidekick264 it has to be selected in config.txt on the SD-card by activating  the *include sidekick264_rpi3.txt*-line.
  

The Sidekick264 provides the same menu and browser as the C64/C128 version, and supports PRG loading and C1low/C1high cartridges. As a bonus it can be used as Dual-SID-card (addresses $FD40 and $FE80), as FM-card (I chose address $FDE2), can emulate TED-sound and Digiblaster output. And -- until recently without any use case :-) -- a Geo/NeoRAM-compatible memory expansion (registers at $FDE8-$FDEA, memory window at $FE00-$FE7F). The Sidekick264 repository contains Alpharay and Pet's Rescue modified to run directly off the emulated NeoRAM! The SD Card contains example programs with source to demonstrate these functionalities.

  

**Attention:** Sidekick264 is currently tested with RPi 3A+ and 3B+ only and requires to overclock your RPi (see SD:sidekick264.txt). Please be aware that this may void warranty. In principle it also works with a RPi Zero 2, however, it requires insane overclocking and which usually refuses to work.

Note that for resetting a Plus/4 you will need to press the computer's own reset button instead of the one on the Sidekick PCB. This is annoying and because the Plus/4 does not reboot on a reset signal at the expansion port.

There is a simple hardware fix which would require to bridge pin 2 and 12 of a 7406 to make /RESET = /BRESET (thanks for this hint kinzi!), e.g. using a wire with test hooks. The Sidekick software will work around if you use the computer's reset button.

**Note:** if you want to emulate cartridges using the C1Hi-range you will have to connect the pin "C1 Hi" on the adapter to the "SID CS"-pin on the Sidekick (there are only few cartridges for C16, and even less use high ROM). Again, please double check that you set the d=3 jumper on the Sidekick64 PCB (see above).

  

## Sidekick20 for VIC20

Sidekick20 also requires a passive adapter (Gerber files and schematics are in the repo) to connect the Sidekick64 to the expansion port. Note there are two variants for horizontal and vertical positioning of the Sidekick-PCB. The interactive BOMs are [this](https://htmlpreview.github.io/?https://github.com/frntc/Sidekick64/blob/master/Gerber/ibom_sk20.html) and [this](https://htmlpreview.github.io/?https://github.com/frntc/Sidekick64/blob/master/Gerber/ibom_sk20v.html).

### Setup and Raspberry Pi models

To enable Sidekick20 it has to be selected in *config.txt* on the SD-card by activating the *include sidekick20_rpi0.txt*-line. If this works, you can skip over to the next section. If it doesn't, read on. 

The Raspberry Pi clock settings are set in *sidekick20*.txt* in the SD-card root folder, and bus timings in *VC20/sidekick.cfg* (again, note that overclocking voids warranty). There are three settings/files as reference points and I recommend to try the following order of settings: 
+ the default is choosing *sidekick20_rpi0.txt* in *config.txt* and the *consolidated timings* in VC20/sidekick20.cfg (these are default settings for the RPi Zero 2, but normally work on other models as well). 
+ if this doesn't work and you use a RPi 3A+/3B+ choose *sidekick20_rpi3.txt* and *timings for 1.4GHz RPi 3* 
+ if this still doesn't work (I'd suspect the problem being somewhere else, but) try the even higher clock settings.

The reasons for exposing the timings stem from the limited number of machines which are available to me for testing and thus some remaining uncertainty about the ideal timings. Also be aware that VIC-emulation is very performance critical as it is cycle-exact, i.e. the emulated-VIC is in lockstep with the real one (there is no delay other than that of your HDMI-display setup), and immediately outputs to the HDMI frame buffer. This means, you're probably less likely to experience instabilities when the emulation is deactivated.  
 
### VIC-emulation (HDMI output)

**Note:** to use the VIC-emulation (i.e. VIC output via HDMI) you have to connect the A15-signal, e.g. from the 6502 CPU pin 25, to the pin labelled "in 1" on the adapter. This signal is not available at the expansion port, but necessary to track VIC read/write accesses.

There are some special configuration settings for Sidekick20 (in VC20/sidekick20.cfg):
- *VIC_EMULATION* disables the emulation (setting it to *NONE*) or enables it by setting your system's video standard, i.e. *PAL* or *NTSC*
- the HDMI output can simulate simple scanlines, you can set their default brightness (0=black, 256=no visible scanline) with *VIC_SCANLINE_INTENSITY*. The intensity can be changed at runtime by pressing '+' and '-'.
- to enable the support for the VFLI graphics mode (208x256 pixels, 16 colors) - which normally requires quite some hardware modification, simply enable it with *VIC_VFLI_SUPPORT YES* (or disable it with *NO*)
- the VIC audio output can undergo some filtering (as on the real machine) which can be controlled by setting *VIC_AUDIO_FILTER* to *YES* or *NO*

Again, if you do not connect A15, the VIC-emulation will not work/be disabled.

The Sidekick20-menu supports two font sizes (5x8 and 6x8) which you can choose by setting *FONT_WIDTH* to *NARROW* (38 column screen) or *WIDE* (32 column screen). Don't forget to adjust the positioning of the main menu entries accordingly.

**Note:** ideally you configure the Raspberry Pi's HDMI-mode consistent with your PAL/NTSC setting above. This can be done using the "hdmi_mode" in sidekick20*.txt on the SD-card.


### Drive emulation
The Sidekick20 emulates LOAD and SAVE via kernal vectors, i.e. single-file programs and programs that use kernal routines LOAD/SAVE only work fine (for other programs use a real drive, Pi1541 or SD2IEC). The drive emulation exposes the very same folder structure as shown in the browser. When LOADing a sub-folder or D64-file (which appear in the directory) you change into it, e.g. *LOAD"game.d64",9* (replace "9" by your drive letter if you changed it). To go one level up in the file hierarchy use *LOAD"&larr;",9* , and to go to the root folder use *LOAD"//",9*.
  

## How does it work? (technical details)
Connecting the computers to the RPi requires level shifting to interface the 5V bus with the 3.3V GPIOs of the RPi. However, things get a bit more complicated as communication on the data lines (obviously) needs to be bidirectional and there are way too few GPIOs on a standard RPi to simply connect to all signals. The Sidekick64 reads A0-A13, IO1, IO2, ROML, ROMH, Phi2, Reset, HIRAM and SID-chipselect,

R/W and reads/writes data lines D0-D7 (plus GPIOs for controlling the circuitry). This makes the use of multiplexers necessary. Additionally it also controls GAME, EXROM, NMI, DMA and RESET, and, very important :-), drive an OLED or TFT display and LEDs.

  

On the software side, handling the communication is time critical. The communication needs to happen within a time window of approx. 500 nanoseconds (significantly less on a C16/+4). This includes reading the GPIOs, switching multiplexers and reading again, possibly figuring out which data to put on the bus, doing so -- and getting hands off again on time. Many of these steps obviously must not happen too late, but also not too early (e.g. some signals might not be ready). Just sniffing the bus, e.g. getting SID commands and running an emulation is less time critical than reacting, for instance when emulating a cartridge. In addition to careful timing – and this turned out to be the most complicated part on the RPi – we also must avoid cache misses.

  

I implemented the communication in a fast interrupt handler (FIQ) which handles GPIO reading, preloads caches if required, and reacts properly. The timing is done using the ARM's instruction counters. I use Circle (https://github.com/rsta2/circle) for bare metal programming the RPi which provides a great basis and great support by its creator Rene Stange.

  

## Known limitations/bugs

  

Please keep in mind that you're not reading about a product, but my personal playground that I'm sharing. Not all kinds of .CRTs are supported. Not all kinds of disk images are supported (I only tried D64, D71 might work, D81 not). At, hopefully very rare occasions, there might be some glitches (although they should be mostly eliminated over the software iterations) as the RPis are really not made for what they are used here.

  

## Building the code (if you want to)

Setup your Circle44.3 and gcc-arm environment, then you can compile Sidekick64 almost like any other example program (the repository contains the build settings for Circle that I use -- make sure you use them, otherwise it will probably not work). Use "make -kernel={sid|cart|ram|ef|fc3|ar|menu|menu20|menu264}" to build the different kernels, then put the kernel together with the Raspberry Pi firmware on an SD(HC) card with FAT file system and boot your RPi with it (the "menu"-kernels are the aforementioned main software). The C64/C16/VIC20 code is compiled using cc65 and 64tass.

  

## Videos

I added two videos showing life captures of the C64 screen where the RPi is used as a "graphics coprocessor" for rendering an image using rasterization or path tracing (light transport simulation). This is just meant to show case that the RPi can be used for more than just emulating existing cartridges/hardware. The source code is not yet included in the repository, contact me if you are interested.

## Disclaimer

Be careful not to damage your RPi or 8-bit computer, or anything attached to it. I am not responsible if you or your hardware gets damaged. In principle, you can attach the RPi and other cartridges at the same time, as long as they do not conflict (e.g. in IO ranges or driving signals). I recommend to NOT do this. For example, using Sidekick64 and an Easyflash 3 will very likely destroy the EF3's CPLD. Again, I'm not taking any responsibility -- if you don't know what you're doing, better don't... use everything at your own risk.

## Where did you get your Sidekick64? How to get one?

You've built it yourself? Cool, this project is for tinkerers!

If you have questions about assembling one, don't hesitate to ask!

If you can't build one yourself, there are official sellers of Sidekick64/RAD/SIDKick: www.restore-store.de (all three projects) and [www.retro-updates.com](http://www.retro-updates.com) (Sidekick64).

*It is also perfectly fine and appreciated* if someone sells spare PCBs (populated or not) of a PCB-order or manufactures a small batch and offers them on a forum, but I expect the price tag to be lower than that of the aforementioned official sellers.

If you bought a Sidekick64/RAD/SIDKick for the same price or even more, this clearly is a violation of the license and you should ask the seller why they are not respecting open source/CC developers' licenses.
  

## License

My portions of the source code are work licensed under GPLv3.

The PCB and 3D model/label is work licensed under a Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License.

The software uses third-party libraries and components such as reSID, TinySoundFont, PSID, Disk2EasyFlash, SSD1306xLED, m93c86-emulation from Vice. Please see the source folder/files for authors and licenses.  

## Misc

Last but not least I would like to thank a few people and give proper credits:

kinzi (forum64.de, F64) for lots of discussions and explanations on electronics and how a C64 actually works. Kim Jørgensen for chats on weird bus timings and freezers, letting me adapt his C64-side code of KFF's EAPI for use with Sidekick64 and last but not least for joining for SK20, in particular for the VIC20 disk emulation. Thanks also go to Roland Hermans for creating and sharing PSID64, and to ALeX for letting me use his Disk2EasyFlash. Also, thanks to the beta testers on F64 (in particular emulaThor, bigby, kinzi and TurboMicha).

Rene Stange (the author of Circle) for his framework and patiently answering questions on it, and digging into special functionality (e.g. ARM stubs without L1 prefetching). Retrofan (https://compidiaries.wordpress.com/) for sharing his new system font which is also used in all recent releases, and for the Sidekick logo. And of course thanks a lot to Mad^BKN for porting Alpharay and Pet's Rescue in an amazingly short time!

The authors of reSID and the OPL emulators (also used in WinVICE), the authors of SSD1306xLED (https://bitbucket.org/tinusaur/ssd1306xled, which I modified to work in this setting) for making their work available. The code in the repo further builds on d642prg V2.09 and some other code fragments found on cbm-hackers and codebase64.org. Also thanks and greetings fly to Arnaud Carré (aka Leonard/Oxygene) for his StSound library which SK64 uses to play YM files (https://github.com/arnaud-carre/StSound), and to rombankzero for pocketmod (https://github.com/rombankzero/pocketmod) used to render MOD-files. The OLED-Sidekick logo is based on a font by Atomic Flash found at https://codepo8.github.io/logo-o-matic. The C16/+4 SID and FM examples are based on code by Mr.Mouse/Xentax and Antti Hannula's (flex) 2sid tune "Eternity" (original mod by Matt Simmonds); the FM songs are Koere and Like Galway by Jesper Olson (jo). The C16 cartridge startup code is based on CBMHARDWARE's example.

  
  

### Trademarks

  

Raspberry Pi is a trademark of the Raspberry Pi Foundation.

  
  
