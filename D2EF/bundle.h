#ifndef BUNDLE_H
#define BUNDLE_H

#include <stdint.h>
#include <stdio.h>

#include "m2i.h"

extern void bundle(unsigned char **out, struct m2i *entries, 
		uint32_t bank_size, uint32_t bank_offset, uint32_t bank_shift, 
		uint8_t *api, uint32_t api_length, uint8_t *launcher, uint32_t launcher_length,
		uint16_t first_bank
);

#endif