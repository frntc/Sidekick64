
#ifndef CART_H
#define CART_H

#include <stdint.h>

// Reference: http://ist.uwaterloo.ca/~schepers/formats/CRT.TXT
typedef struct CartHeader_s
{
    // Cartridge signature "C64 CARTRIDGE" (padded with space chars)
    char signature[16];

    // File header length  ($00000040,  high/low)
    uint8_t headerLen[4];

    // Cartridge version (high/low, presently 01.00)
    uint8_t version[2];

    // Cartridge hardware type ($0000, high/low)
    uint8_t type[2];

    // Cartridge port EXROM line status (1 = active)
    uint8_t exromLine;

    // Cartridge port GAME line status (1 = active)
    uint8_t gameLine;

    // Reserved for future use (6 bytes)
    uint8_t reserved[6];

    // 32-byte cartridge name (uppercase,  padded with 0)
    char name[32];
}
CartHeader;


typedef struct BankHeader_s
{
    // Contained ROM signature "CHIP"
    char signature[4];

    // Total packet length, ROM image size + header (high/low format)
    uint8_t packetLen[4];

    // Chip type: 0 - ROM, 1 - RAM, no ROM data, 2 - Flash ROM
    uint8_t chipType[2];

    // Bank number ($0000 - normal cartridge) (?)
    uint8_t bank[2];

    // Starting load address (high/low format) (?)
    uint8_t loadAddr[2];

    // ROM image size (high/low format, typically $2000 or $4000)
    uint8_t romLen[2];
}
BankHeader;


// "C64 CARTRIDGE   "
#define CART_SIGNATURE { 0x43, 0x36, 0x34, 0x20, 0x43, 0x41, 0x52, 0x54, 0x52, 0x49, 0x44, 0x47, 0x45, 0x20, 0x20, 0x20 }
#define CHIP_SIGNATURE { 0x43, 0x48, 0x49, 0x50 }

// These are the cartridge types from the file header
#define CART_TYPE_EASYFLASH       32
#define CART_TYPE_EASYFLASH_XBANK 33

#define MAX_BANKS 63
// one less than the easyflash can, because we need space to add a lancher

#endif // CART_H
