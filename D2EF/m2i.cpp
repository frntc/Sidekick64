#include <stdlib.h>
#include <stdio.h>
//#include "getopt.h"
#include <errno.h>
//#include <libgen.h>
#include <sys/stat.h>
#include "dirent.h"
#include <string.h>

#include "m2i.h"

struct m2i * parse_m2i(char *filename){
	//char *m2idir = dirname(filename);
	char *m2idir = filename;
	char buffer[1000], filenamebuffer[1000];
	FILE *f;
	struct m2i *first, *last;
	
	f = fopen(filename, "r");
	if(!f){
		fprintf(stderr, "unable to open \"%s\"\n", filename);
		exit(1);
	}
	read_line(f, 1000, buffer);
	first = malloc(sizeof(struct m2i));
	first->next = NULL;
	first->type = '*';
	strncpy(first->name, buffer, 16);
	first->name[16] = '\0';
	strcpy(first->id, "EF 2A");
	last = first;
	
	while(read_line(f, 1000, buffer)){
			if(strlen(buffer) == 0 || buffer[0] != '#'){
				struct m2i *entry = malloc(sizeof(struct m2i));
				entry->next = NULL;
				last->next = entry;
				last = entry;
				int col1, col2, i;
				// find the first two colons
				for(col1 = 0; col1 < strlen(buffer)-1 && buffer[col1] != ':'; col1++);
				for(col2 = col1+1; col2 < strlen(buffer)-1 && buffer[col2] != ':'; col2++);
				// convert the colons in <end of line> and go to next char
				buffer[col1++] = '\0';
				buffer[col2++] = '\0';
				// chop off spaces at the end
				for(i=strlen(&buffer[col1]); i > 0 && buffer[col1+i-1] == ' '; i--);
				buffer[col1+i] = '\0';
				for(i=strlen(&buffer[col2]); i > 0 && buffer[col2+i-1] == ' '; i--);
				buffer[col2+i] = '\0';
				// quick check
				if(strlen(buffer) == 0 || strlen(&buffer[col2]) == 0){
					fprintf(stderr, "error while parsing \"%s\"\n", buffer);
					exit(1);
				}
				// create entry
				entry->type = tolower(buffer[0]);
				if(entry->type != 'd' && entry->type != 'p' && entry->type != 'q'){
					fprintf(stderr, "bad type: \"%s\"\n", buffer);
					exit(1);
				}
				strncpy(entry->name, &buffer[col2], 16);
				entry->name[16] = '\0';
				if(entry->type == 'p' || entry->type == 'q'){
					FILE *df;
					strcpy(filenamebuffer, m2idir);
					strcat(filenamebuffer, "/");
					strcat(filenamebuffer, &buffer[col1]);
					df = fopen(filenamebuffer, "rb");
					if(!df){
						fprintf(stderr, "unable to open file: \"%s\" (of line \"%s\")\n", filenamebuffer, buffer);
						exit(1);
					}
					fseek(df, 0, SEEK_END);
					entry->length = ftell(df);
					fseek(df, 0, SEEK_SET);
					entry->data = malloc(entry->length);
					fread(entry->data, 1, entry->length, df);
					fclose(df);
				}
			}
	}
	
	fclose(f);
	
	return first;
}

int read_line(FILE *f, int maxlength, char* buffer){
	static int next_char = -1;
	int i=0;
	
	while(i < maxlength-1){
		int ch = next_char >= 0 ? next_char : fgetc(f);
		next_char = -1;
		if(ch < 0){
			if(i == 0)
				return 0;
			else
				break;
		}
		if(ch == '\n' || ch == '\0'){
			// found an newline
			break;
		}
		if(ch == '\r'){
			// found a carrige return
			ch = fgetc(f);
			if(ch != '\n')
				next_char = ch;
			break;
		}
		buffer[i++] = ch;
	}
	buffer[i] = '\0';
	return 1;
}
