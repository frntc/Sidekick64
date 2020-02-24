#
# Makefile
#
EXTRACLEAN = OLED/*.o resid/*.o resid_vice_trunk/*.o

CIRCLEHOME = ../..

ifeq ($(sid), resid_vice)
RESIDLOC = resid_vice_trunk
RESIDFILTER = filter.o
else ifeq ($(sid), resid_vice_nf)
RESIDLOC = resid_vice_trunk
RESIDFILTER = filter8580new.o
CFLAGS += -DNEW_8580_FILTER
else
RESIDLOC = resid
RESIDFILTER = filter.o
endif

OBJS = lowlevel_arm64.o gpio_defs.o helpers.o latch.o oled.o ./OLED/ssd1306xled.o ./OLED/ssd1306xled8x16.o ./OLED/num2str.o 

ifeq ($(kernel), menu)
CFLAGS += -DCOMPILE_MENU=1
OBJS += kernel_menu.o kernel_kernal.o kernel_launch.o kernel_ef.o kernel_fc3.o kernel_ar.o crt.o dirscan.o config.o kernel_rkl.o c64screen.o

CFLAGS += -DCOMPILE_MENU_WITH_SOUND=1
OBJS += kernel_sid.o sound.o ./$(RESIDLOC)/dac.o ./$(RESIDLOC)/$(RESIDFILTER) \
				./$(RESIDLOC)/envelope.o ./$(RESIDLOC)/extfilt.o ./$(RESIDLOC)/pot.o \
				./$(RESIDLOC)/sid.o ./$(RESIDLOC)/version.o ./$(RESIDLOC)/voice.o \
				./$(RESIDLOC)/wave.o fmopl.o
CFLAGS += -DUSE_VCHIQ_SOUND=$(USE_VCHIQ_SOUND) 

LIBS	= $(CIRCLEHOME)/addon/vc4/sound/libvchiqsound.a \
   	      $(CIRCLEHOME)/addon/vc4/vchiq/libvchiq.a \
	      $(CIRCLEHOME)/addon/linux/liblinuxemu.a
endif

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
OBJS += kernel_sid.o sound.o ./$(RESIDLOC)/dac.o ./$(RESIDLOC)/$(RESIDFILTER) \
				./$(RESIDLOC)/envelope.o ./$(RESIDLOC)/extfilt.o ./$(RESIDLOC)/pot.o \
				./$(RESIDLOC)/sid.o ./$(RESIDLOC)/version.o ./$(RESIDLOC)/voice.o \
				./$(RESIDLOC)/wave.o fmopl.o 
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
