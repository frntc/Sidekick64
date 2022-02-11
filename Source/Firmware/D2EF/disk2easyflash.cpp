#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <sys/stat.h>
#include <string.h>

#include "d64.h"
#include "bundle.h"
#include "binaries.h"
#include "cart.h"

#define mode_lo 0
#define mode_nm 1
#define mode_hi 2

#define build_none 0
#define build_xbank 1
#define build_crt 2
#define build_list 3

//unsigned char cart[ 1024 * 1024 + 128 ];
//unsigned char diskimage[ 1024 * 1024 ];
//int imageSize;

int createD2EF( unsigned char *diskimage, int imageSize, unsigned char *cart, int build, int mode, int autostart )
{
/*	build = build_crt;
	mode = mode_nm;
	int autostart = 0;*/

	static char cart_singnature[] = { 0x43, 0x36, 0x34, 0x20, 0x43, 0x41, 0x52, 0x54, 0x52, 0x49, 0x44, 0x47, 0x45, 0x20, 0x20, 0x20 };
	static char chip_singnature[] = { 0x43, 0x48, 0x49, 0x50 };

	char txt_blocks_free[] = "BLOCKS FREE.";
	char txt_prg[] = "PRG";
	char txt_del[] = "DEL";

	int i;
	int nolisting = 0;
	int blocksfree = 2;
	//int verbose = 0;
	//struct stat m2i_stat;
	struct m2i *entries, *entr;
	unsigned char *out;

	CartHeader CartHeader;

	out = cart;

	entries = parse_d64(diskimage, imageSize);
	
	if(!nolisting){
		struct m2i *entry = (struct m2i *)malloc(sizeof(struct m2i));
		int num;
		
		// search last entry
		for(num = 0, entr = entries; entr != NULL; num++, entr = entr->next);
		
		// fill up entry
		entry->next = NULL;
		strcpy(entry->name, "$");
		entry->type = 'p';
		entry->data = (uint8_t*)malloc(50 + num * 30);
		entry->length = 0;
		
		// basic setup 
		entr = entries;
		
		entry->data[entry->length++] = 0x01; // laod address
		entry->data[entry->length++] = 0x04;
		
		// first line
		entry->data[entry->length++] = 0x01; // next ptr
		entry->data[entry->length++] = 0x01;
		entry->data[entry->length++] = 0x00; // size
		entry->data[entry->length++] = 0x00;
		entry->data[entry->length++] = 0x12; // REVERSE
		entry->data[entry->length++] = 0x22; // QUOTE
		for(i=0; i<strlen(entr->name); i++){
			entry->data[entry->length++] = entr->name[i]; // name
		}
		for(; i<16; i++){
			entry->data[entry->length++] = 0x20; // SPACE
		}
		entry->data[entry->length++] = 0x22; // QUOTE
		entry->data[entry->length++] = 0x20; // SPACE
		for(i=0; i<5; i++){
			entry->data[entry->length++] = entr->id[i]; // name
		}
		entry->data[entry->length++] = 0x00; // END OF LINE
		
		for(entr = entries->next; entr != NULL; entr = entr->next){
			char *type = entr->type == 'd' ? txt_del : txt_prg;
			int size = (entr->length + 253) / 254;
			
			// entry
			entry->data[entry->length++] = 0x01; // next ptr
			entry->data[entry->length++] = 0x01;
			entry->data[entry->length++] = size; // size
			entry->data[entry->length++] = size >> 8;
			
			// convert size -> 4-strlen("size")
			if(size < 10){
				size = 3;
			}else if(size < 100){
				size = 2;
			}else{
				size = 1;
			}
			
			for(i=0; i<size; i++){
				entry->data[entry->length++] = 0x20; // SPACE
			}
			
			entry->data[entry->length++] = 0x22; // QUOTE
			for(i=0; i<strlen(entr->name); i++){
				entry->data[entry->length++] = entr->name[i]; // name
			}
			entry->data[entry->length++] = 0x22; // QUOTE
			for(; i<17; i++){
				entry->data[entry->length++] = 0x20; // SPACE
			}
			for(i=0; i<3; i++){
				entry->data[entry->length++] = type[i]; // type (PRG/DEL)
			}
			entry->data[entry->length++] = 0x00; // END OF LINE
			
		}
		
		// last line
		entry->data[entry->length++] = 0x01; // next ptr
		entry->data[entry->length++] = 0x01;
		entry->data[entry->length++] = blocksfree; // size
		entry->data[entry->length++] = blocksfree >> 8;
		for(i=0; i<strlen(txt_blocks_free); i++){
			entry->data[entry->length++] = txt_blocks_free[i]; // name
		}
		entry->data[entry->length++] = 0x00; // END OF LINE
		
		// end of file		
		entry->data[entry->length++] = 0x00; // END OF FILE
		entry->data[entry->length++] = 0x00;
		
		// search last entry, add
		for(entr = entries; entr->next != NULL; entr = entr->next);
		entr->next = entry;
		
	}
	
	
	
	
	/*if(build == build_list || verbose){
		char quotename[19];
		int num;
		
		printf("      | 0 \"%-16s\" %s |\n", entries->name, entries->id);
		
		for(num=1, entr = entries->next; nolisting ? (entr != NULL) : (entr->next != NULL); num++, entr = entr->next){
			if(entr->type == '*' || (entr->type != 'p' && nolisting)){
				continue;
			}
			
			//		printf("%c:%s:%d\n", entr->type, entr->name, entr->length);
			
			sprintf(quotename, "\"%s\"", entr->name);
			
			// Print directory entry 
			printf("%4d  | %-4d  %-18s %s |  %5d Bytes%s\n", num, (entr->length+253)/254, quotename, entr->type == 'd' ? "DEL" : "PRG", entr->length, entr->type == 'q' ? ", not in flash, only in listing" : "");
			
		}
		
		if(build != build_list && entr != NULL){
			// the listing
			printf("      |       The Listing (\"$\")      |  %5d Bytes\n", entr->length);
		}
		
	}*/
	
	if(build != build_list){
		
		/*out = fopen("test.crt", "wb");
		if(!out){
			fprintf(stderr, "unable to open \"%s\" for output", argv[1]);
			exit(1);
		}*/
		
		memcpy(CartHeader.signature, cart_singnature, 16);
		CartHeader.headerLen[0] = 0x00;
		CartHeader.headerLen[1] = 0x00;
		CartHeader.headerLen[2] = 0x00;
		CartHeader.headerLen[3] = 0x40;
		CartHeader.version[0] = 0x01;
		CartHeader.version[1] = 0x00;
		if(build == build_xbank){
			CartHeader.type[0] = CART_TYPE_EASYFLASH_XBANK >> 8;
			CartHeader.type[1] = CART_TYPE_EASYFLASH_XBANK & 0xff;
			CartHeader.exromLine = (mode == mode_hi) ? 1 : 0;
			CartHeader.gameLine = (mode == mode_hi) ? 0 : ((mode == mode_lo) ? 1 : 0);
		}else{
			CartHeader.type[0] = CART_TYPE_EASYFLASH >> 8;
			CartHeader.type[1] = CART_TYPE_EASYFLASH & 0xff;
			CartHeader.exromLine = 1;
			CartHeader.gameLine = 0;
		}
		CartHeader.reserved[0] = 0;
		CartHeader.reserved[1] = 0;
		CartHeader.reserved[2] = 0;
		CartHeader.reserved[3] = 0;
		CartHeader.reserved[4] = 0;
		CartHeader.reserved[5] = 0;
		memset(CartHeader.name, 0, 32);
		
//		strncpy(CartHeader.name, basename(argv[0]), 32);
		strncpy(CartHeader.name, "D2EF", 32);
		
		//fwrite(&CartHeader, 1, sizeof(CartHeader), out);
		memcpy( out, &CartHeader, sizeof(CartHeader) );
		out += sizeof(CartHeader);
		
		if(build == build_crt){
			char buffer[0x2000];
			BankHeader BankHeader;
			
			memset(buffer, 0xff, 0x2000);
			memcpy(buffer+0x1800, sprites, sprites_size);
			memcpy(buffer+0x1e00, startup, startup_size);
			buffer[0x2000-startup_size+0] = 1; // BANK
			switch(mode){
				case mode_nm:
					buffer[0x2000-startup_size+1] = 7; // MODE
					break;
				case mode_lo:
					buffer[0x2000-startup_size+1] = 6; // MODE
					break;
				case mode_hi:
					buffer[0x2000-startup_size+1] = 5; // MODE
					break;
			}
			// setup commons	
			memcpy(BankHeader.signature, chip_singnature, 4);
			BankHeader.packetLen[0] = 0x00;
			BankHeader.packetLen[1] = 0x00;
			BankHeader.packetLen[2] = 0x20;
			BankHeader.packetLen[3] = 0x10;
			BankHeader.chipType[0] = 0x00;
			BankHeader.chipType[1] = 0x00;
			BankHeader.bank[0] = 0x00;
			BankHeader.bank[1] = 0x00;
			BankHeader.loadAddr[0] = 0xa0;
			BankHeader.loadAddr[1] = 0x00;
			BankHeader.romLen[0] = 0x20;
			BankHeader.romLen[1] = 0x00;
			
			//fwrite(&BankHeader, 1, sizeof(BankHeader), out);
			memcpy( out, &BankHeader, sizeof(BankHeader) );
			out += sizeof(BankHeader);
			
			//fwrite(buffer, 1, 0x2000, out);
			memcpy( out, buffer, 0x2000 );
			out += 0x2000;
		}
		
		switch(mode){
			case mode_nm:
				if ( autostart )
					bundle(&out, entries, 0x4000, 0x8000, 14, kapi_nm_auto, kapi_nm_size_auto, NULL, 0, build == build_xbank ? 0 : 1); else
					bundle(&out, entries, 0x4000, 0x8000, 14, kapi_nm, kapi_nm_size, NULL, 0, build == build_xbank ? 0 : 1);
				break;
			case mode_lo:
				if ( autostart )
					bundle(&out, entries, 0x2000, 0x8000, 13, kapi_lo_auto, kapi_lo_size_auto, NULL, 0, build == build_xbank ? 0 : 1); else
					bundle(&out, entries, 0x2000, 0x8000, 13, kapi_lo, kapi_lo_size, NULL, 0, build == build_xbank ? 0 : 1);
				break;
			case mode_hi:
				if ( autostart )
					bundle(&out, entries, 0x2000, 0xa000, 13, kapi_hi_auto, kapi_hi_size_auto, launcher_hi, launcher_hi_size_auto, build == build_xbank ? 0 : 1); else
					bundle(&out, entries, 0x2000, 0xa000, 13, kapi_hi, kapi_hi_size, launcher_hi, launcher_hi_size, build == build_xbank ? 0 : 1);
				break;
		}
		
	}

	return out-cart;
}

/*
int main(int argc, char** argv)
{
	FILE *f = fopen( "test.d64", "rb" );
	fseek( f, 0, SEEK_END );
	imageSize = ftell( f );
	fseek( f, 0, SEEK_SET );
	fread( diskimage, 1, imageSize, f );
	fclose( f );

	int crtSize = createD2EF( diskimage, imageSize, cart, build_crt, mode_nm, 1 );

	f = fopen("test.crt", "wb");
	fwrite( cart, 1, crtSize, f );
	fclose( f );

	exit(0);
}
*/