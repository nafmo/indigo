// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or (at your option)
// any later version.

// You should have received a copy of the GNU General Public License along
// with this program; if not, write to the Free Software Foundation, Inc., 675
// Mass Ave, Cambridge, MA 02139, USA.

// shorttag.cpp
// Truncate an echotag so that it fits the Blue Wave .inf record.

#include "crc32.h"
#include "shorttag.h"
#include "datatyp.h"
#include <string.h>
#include <stdio.h>

// makeShortTag
// Truncate an echotag so that it fits the Blue Wave .inf record. Since
// there might be clashes if we just chopped it off, we calculate a
// crc value on the tag, and include it as characters.
const char *makeShortTag(const char *longTag)
{
    // Initialize our short tag.
    static char crctag[21] = " ";

    // If the tag is already short enough, don't do anything with it.
    if (strlen(longTag) < 21)
        return longTag;

    // Calculate the CRC32 value of the echotag.
    UINT32 tagcrc = 0xffffffff;
    const char *c_p;
    char *c2_p;
    for (c_p = longTag; *c_p; c_p ++)
        tagcrc = UPDC32(*c_p, tagcrc);

    // Convert the CRC32 value to printable characters.
    for (c2_p = &crctag[1]; tagcrc; tagcrc >>= 6)
        *(c2_p ++) = (tagcrc & 0x3F) + '!';
    // And then put what of the echotag that fits after it. This should
    // make the tags unique.
    for (c_p = longTag; c2_p < &crctag[20];)
        *(c2_p ++) = *(c_p ++);
    *c2_p = 0;

    // Return the truncated tag.
    return crctag;
}
