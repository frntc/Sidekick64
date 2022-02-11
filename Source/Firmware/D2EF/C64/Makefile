
all: binaries.c

kapi: kapi_hi.prg kapi_lo.prg kapi_nm.prg
launcher: launcher_hi.bin

DEPDIR = .
df = $(DEPDIR)/$(*F)

SRCS = kapi_hi.asm kapi_lo.asm kapi_nm.asm

N = "\\033[0m"
B = "\\033[1m"

copyright.bin: copyright.php
	@echo "$(B)Assembling $@$(N)"
	php copyright.php

%.prg: %.asm
	@echo "$(B)Assembling $<$(N)"
	php mkdep.php $< > $(df).P
	java -jar KickAss2.25/KickAss.jar -showmem -vicesymbols -o $@ $<

launcher_hi.bin: launcher_hi.asm
	@echo "$(B)Assembling $<$(N)"
	java -jar KickAss3.42/KickAss.jar -binfile -showmem launcher_hi.asm -o launcher_hi.bin

binaries.c: kapi launcher
	@echo "$(B)Assembling $@$(N)"
	php binaries.php

# autodep

-include $(SRCS:%.asm=$(DEPDIR)/%.P)
