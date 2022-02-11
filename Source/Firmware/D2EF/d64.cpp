#include <stdlib.h>
#include <stdio.h>
//#include "getopt.h"
#include <errno.h>
//#include <libgen.h>
#include <sys/stat.h>
//#include "dirent.h"
#include <string.h>

#include "m2i.h"
#include "diskimage.h"

//struct m2i * parse_d64(char *filename){
struct m2i * parse_d64(unsigned char *image, int imageSize){
	struct m2i *first, *last;
	DiskImage *di;
	ImageFile *dh, *fdh;
	unsigned char buffer[254];
	int offset;
	//	FILE *f;
	
	first = (m2i*)malloc(sizeof(struct m2i));
	first->next = NULL;
	first->type = '*';
	last = first;
	
	/* Load image into ram */
//	if ((di = di_load_image(filename)) == NULL) {
	if ((di = di_load_image(image, imageSize)) == NULL) {
		//fprintf(stderr, "unable to open \"%s\"\n", filename);
		//exit(1);
	}
	
	/* Convert title */
	di_name_from_rawname(first->name, di_title(di));
	
	/* Convert ID */
	memcpy(first->id, di_title(di) + 18, 5);
	first->id[5] = 0;
	
	/* Open directory for reading */
	if ((dh = di_open(di, (unsigned char *) "$", T_PRG, "rb")) == NULL) {
		//fprintf(stderr, "unable to read directory \"%s\"\n", filename);
		//exit(1);
	}
	
	/* Read first block into buffer */
	if (di_read(dh, buffer, 254) != 254) {
		//fprintf(stderr, "unable to read bam \"%s\"\n", filename);
		//exit(1);
	}
	
	/* Read directory blocks */
	while (di_read(dh, buffer, 254) == 254) {
		for (offset = -2; offset < 254; offset += 32) {
			
			/* If file type != 0 */
			if (buffer[offset+2]) {
				
				struct m2i *entry = (m2i*)malloc(sizeof(struct m2i));
				
				entry->next = NULL;
				di_name_from_rawname(entry->name, buffer + offset + 5);
				
				//	type = buffer[offset + 2] & 7;
				//	closed = buffer[offset + 2] & 0x80;
				//	locked = buffer[offset + 2] & 0x40;
				//	size = buffer[offset + 31]<<8 | buffer[offset + 30];
				
				switch(buffer[offset + 2] & 7){
					case 0: // DEL
						entry->type = 'd';
						break;
					case 2: // PRG
						entry->type = 'p';
						break;
					default:
						//fprintf(stderr, "unsuported type %d", buffer[offset + 2] & 7);
						//exit(1);
						break;
				}
				
				if(entry->type == 'd'){
					entry->length = 0;
					last->next = entry;
					last = entry;
				}else{
#define temp_space (1024*1024)
					int len;
					
					entry->data = (uint8_t*)malloc(temp_space);
					entry->length = 0;
					
					if ((fdh = di_open(di, buffer + offset + 5, (FileType)(buffer[offset + 2] & 7), "rb")) == NULL) {
						//fprintf(stderr, "warning: unable to open file \"%s\" in d64 \"%s\"\n", last->name, filename);
					}else{
						
						while ((len = di_read(fdh, &entry->data[entry->length], temp_space - entry->length)) > 0) {
							entry->length += len;
						}
						
						entry->data = (uint8_t*)realloc(entry->data, entry->length);
						
						di_close(fdh);
						last->next = entry;
						last = entry;
						
					}
					
				}
				
			}
		}
	}
	
	
	di_close(dh);
	di_free_image(di);
	
	return first;
}
