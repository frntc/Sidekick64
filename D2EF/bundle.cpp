#include <stdlib.h>
#include <stdio.h>
//#include "getopt.h"
#include <errno.h>
//#include <libgen.h>
#include <sys/stat.h>
//#include "dirent.h"
#include <string.h>

//#include "m2i.h"
#include "bundle.h"
#include "cart.h"

static unsigned char toupper( unsigned char c )
{
	if ( c >= 'a' && c <= 'z' )
		return c + 'A' - 'a';
	return c;
}


uint8_t flash_memory[MAX_BANKS * 0x4000];

struct dir_entry {
	char name[16];
	uint8_t bank[2]; // lo/hi
	uint8_t offset[2]; // lo/hi
	uint8_t loadaddress[2]; // lo/hi
	uint8_t size[2]; // lo/hi
};

void bundle(unsigned char **out, struct m2i *entries, 
			uint32_t bank_size, uint32_t bank_offset, uint32_t bank_shift, 
			uint8_t *api, uint32_t api_length, uint8_t *launcher, uint32_t launcher_length,
			uint16_t first_bank
			){
	uint32_t pos = 0, dir_pos;
	struct m2i *entr;
	int i, j;
	BankHeader BankHeader;
	
	// set everything to 0xff
	memset(flash_memory, 0xff, MAX_BANKS * 0x4000);
	
	// add the api
	memcpy(&flash_memory[pos], api, api_length);
	pos += api_length;
	
	// add the visible line
	for(i=strlen(entries->name); entries->name[i-1] == 0x20; i--);
	entries->name[i] = '\0';
	i = (31-strlen(entries->name)) / 2;
	memset(&flash_memory[pos], 0x20, 40);
	flash_memory[pos+i+0] = '*';
	flash_memory[pos+i+1] = '*';
	flash_memory[pos+i+2] = '*';
	flash_memory[pos+i+3] = '*';
	for(j=0; j<strlen(entries->name); j++){
		if(toupper(entries->name[j]) >= 0x41 && toupper(entries->name[j]) <= 0x5a){
			flash_memory[pos+i+5+j] = toupper(entries->name[j]) - 0x40;
		}else{
			flash_memory[pos+i+5+j] = toupper(entries->name[j]);
		}
	}
	flash_memory[pos+i+5+strlen(entries->name)+1] = '*';
	flash_memory[pos+i+5+strlen(entries->name)+2] = '*';
	flash_memory[pos+i+5+strlen(entries->name)+3] = '*';
	flash_memory[pos+i+5+strlen(entries->name)+4] = '*';
	pos += 40;
	
	// setup dir_pos (but the dir is still empty)
	dir_pos = pos;
	for(i=0, entr = entries->next; entr != NULL; entr=entr->next){
		if(entr->type == 'p'){
			i++;
		}
	}
	pos += i*24;
	
	// setup "end of dir marker"
	flash_memory[pos++] = 0;
	
	// add an ultimax launcher, if needed
	if(launcher != NULL){
		memcpy(&flash_memory[bank_size - launcher_length], launcher, launcher_length);
		pos = bank_size;
	}
	
	// add all files!
	for(entr = entries->next; entr != NULL; entr = entr->next){
		if(entr->type == 'p'){
			// is a PRG, the only thing we store
			uint32_t bank, offset, size;
			struct dir_entry *dir_entry = (struct dir_entry *) (&flash_memory[dir_pos]);
			
			bank = pos >> bank_shift;
			offset = pos & (bank_size-1);
			offset += bank_offset;
			size = entr->length - 2;
			
			if(strlen(entr->name) == 16){
				memcpy(dir_entry->name, entr->name, 16);
			}else{
				strcpy(dir_entry->name, entr->name);
			}
			dir_entry->bank[0] = bank & 0xff;
			dir_entry->bank[1] = bank >> 8;
			dir_entry->offset[0] = offset & 0xff;
			dir_entry->offset[1] = offset >> 8;
			dir_entry->size[0] = size & 0xff;
			dir_entry->size[1] = size >> 8;
			dir_entry->loadaddress[0] = entr->data[0];
			dir_entry->loadaddress[1] = entr->data[1];
			
			memcpy(&flash_memory[pos], &entr->data[2], size);
			pos += size;
			
			dir_pos += sizeof(struct dir_entry);
		}
	}
	
	// setup commons	
	char chip_singnature[] = { 0x43, 0x48, 0x49, 0x50 };

	memcpy(BankHeader.signature, chip_singnature, 4);
	BankHeader.packetLen[0] = 0x00;
	BankHeader.packetLen[1] = 0x00;
	BankHeader.packetLen[2] = 0x20;
	BankHeader.packetLen[3] = 0x10;
	BankHeader.chipType[0] = 0x00;
	BankHeader.chipType[1] = 0x00;
	BankHeader.loadAddr[1] = 0x00;
	BankHeader.romLen[0] = 0x20;
	BankHeader.romLen[1] = 0x00;
	
	for(dir_pos=0; dir_pos<((pos+0x1fff) >> 13); dir_pos++){
		if(bank_size == 0x4000){
			BankHeader.bank[0] = ((first_bank<<1) + dir_pos) >> 9;
			BankHeader.bank[1] = ((first_bank<<1) + dir_pos) >> 1;
			BankHeader.loadAddr[0] = (0x8000 >> 8) + (dir_pos & 1 ? 0x20 : 0x00);
		}else{
			BankHeader.bank[0] = (first_bank + dir_pos) >> 8;
			BankHeader.bank[1] = (first_bank + dir_pos);
			BankHeader.loadAddr[0] = (0x8000 | (bank_offset & 0x2000)) >> 8;
		}
		//fwrite(&BankHeader, 1, sizeof(BankHeader), out);
		memcpy( *out, &BankHeader, sizeof(BankHeader) );
		*out += sizeof(BankHeader);
		
		//fwrite(&flash_memory[dir_pos << 13], 1, 0x2000, out);
		memcpy( *out, &flash_memory[dir_pos << 13], 0x2000 );
		*out += 0x2000;

	}
	
	/*if(bank_size == 0x4000){
		fprintf(stderr, "disk2easyflash conversion finished: %d%s banks (%d KiB) used   (%d bytes used in the last 8 KiB bank)\n",
				dir_pos >> 1,
				(dir_pos & 1) ? ".5" : "",
				dir_pos * 8,
				pos - ((dir_pos-1) << 13)
				);
	}else{
		fprintf(stderr, "disk2easyflash conversion finished: %d banks (%d KiB) used   (%d bytes used in the last 8 KiB bank)\n",
				dir_pos,
				dir_pos * 8,
				pos - ((dir_pos-1) << 13)
				);
	}*/
}
