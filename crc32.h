// crc32.h
// Header file for the CRC32 generation routines.

#ifndef __CRC32_H
#define __CRC32_H

#include "datatyp.h"

extern UINT32 crc_32_tab[];

#define UPDC32(octet,crc) (crc_32_tab[((crc) ^ (octet)) & 0xff] ^ ((crc) >> 8))

#endif
