#
# Makefile
#
EXTRACLEAN = OLED/*.o resid/*.o

CIRCLEHOME = ../..
OBJS = lowlevel_arm64.o gpio_defs.o helpers.o latch.o oled.o ./OLED/ssd1306xled.o ./OLED/ssd1306xled8x16.o ./OLED/num2str.o 

### MENU C64 ###
ifeq ($(kernel), menu)
CFLAGS += -DCOMPILE_MENU=1
OBJS += kernel_menu.o kernel_kernal.o kernel_launch.o kernel_ef.o kernel_fc3.o kernel_ar.o crt.o dirscan.o config.o kernel_rkl.o c64screen.o

CFLAGS += -DCOMPILE_MENU_WITH_SOUND=1
OBJS += kernel_sid.o sound.o ./resid/dac.o ./resid/filter.o ./resid/envelope.o ./resid/extfilt.o ./resid/pot.o ./resid/sid.o ./resid/version.o ./resid/voice.o ./resid/wave.o fmopl.o 
CFLAGS += -DUSE_VCHIQ_SOUND=$(USE_VCHIQ_SOUND) 

LIBS	= $(CIRCLEHOME)/addon/vc4/sound/libvchiqsound.a \
   	      $(CIRCLEHOME)/addon/vc4/vchiq/libvchiq.a \
	      $(CIRCLEHOME)/addon/linux/liblinuxemu.a
endif

### MENU C16/+4 ###
ifeq ($(kernel), menu264)
CFLAGS += -DCOMPILE_MENU=1
OBJS += kernel_menu264.o kernel_launch264.o dirscan.o 264config.o kernel_ramlaunch264.o 264screen.o mygpiopinfiq.o launch264.o

CFLAGS += -DCOMPILE_MENU_WITH_SOUND=1
OBJS += kernel_sid264.o sound.o ./resid/dac.o ./resid/filter.o ./resid/envelope.o ./resid/extfilt.o ./resid/pot.o ./resid/sid.o ./resid/version.o ./resid/voice.o ./resid/wave.o fmopl.o 
CFLAGS += -DUSE_VCHIQ_SOUND=$(USE_VCHIQ_SOUND) 

LIBS	= $(CIRCLEHOME)/addon/vc4/sound/libvchiqsound.a \
   	      $(CIRCLEHOME)/addon/vc4/vchiq/libvchiq.a \
	      $(CIRCLEHOME)/addon/linux/liblinuxemu.a
endif

### individual Kernels for C64 ###
ifeq ($(kernel), cart)
OBJS += kernel_cart.o 
endif

ifeq ($(kernel), launch)
OBJS += kernel_launch.o 
endif

ifeq ($(kernel), kernal)
OBJS += kernel_kernal.o 
endif

ifeq ($(kernel), ef)
OBJS += kernel_ef.o crt.o 
endif

ifeq ($(kernel), fc3)
CFLAGS += -Wl,-emainFC3
OBJS += kernel_fc3.o crt.o 
endif

ifeq ($(kernel), ar)
OBJS += kernel_ar.o crt.o 
endif

ifeq ($(kernel), ram)
OBJS += kernel_georam.o
endif

ifeq ($(kernel), sid)
OBJS += kernel_sid.o sound.o ./resid/dac.o ./resid/filter.o ./resid/envelope.o ./resid/extfilt.o ./resid/pot.o ./resid/sid.o ./resid/version.o ./resid/voice.o ./resid/wave.o fmopl.o 
endif

ifeq ($(kernel), sid)
LIBS	= $(CIRCLEHOME)/addon/vc4/sound/libvchiqsound.a \
   	      $(CIRCLEHOME)/addon/vc4/vchiq/libvchiq.a \
	      $(CIRCLEHOME)/addon/linux/liblinuxemu.a

CFLAGS += -DUSE_VCHIQ_SOUND=$(USE_VCHIQ_SOUND) 
endif

CFLAGS += -Wno-comment

LIBS += $(CIRCLEHOME)/lib/usb/libusb.a \
	    $(CIRCLEHOME)/lib/input/libinput.a \
 	    $(CIRCLEHOME)/addon/SDCard/libsdcard.a \
	    $(CIRCLEHOME)/lib/fs/libfs.a \
		$(CIRCLEHOME)/addon/fatfs/libfatfs.a \
	    $(CIRCLEHOME)/lib/sched/libsched.a \
        $(CIRCLEHOME)/lib/libcircle.a 

include ../Rules.mk
