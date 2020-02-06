PREFIX = aarch64-elf-

no-prefetch.bin: no-prefetch.S
	$(PREFIX)gcc -mcpu=cortex-a53 -c no-prefetch.S
	$(PREFIX)ld -o no-prefetch.elf no-prefetch.o
	$(PREFIX)objdump -D no-prefetch.elf > no-prefetch.lst
	$(PREFIX)objcopy no-prefetch.elf -O binary no-prefetch.bin

clean:
	rm -f *.o *.elf *.bin *.lst
