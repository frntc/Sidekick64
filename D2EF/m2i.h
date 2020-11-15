#ifndef M2I_H
#define M2I_H

#include <stdint.h>
#include <stdio.h>

struct m2i {
	// next entry
	struct m2i *next;
	// m2i data
	char type;
	char name[17];
	char id[6];
	uint8_t *data;
	uint32_t length;
	// later processing
};

struct m2i * parse_m2i(char *filename);

#endif
