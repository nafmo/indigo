// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or (at your option)
// any later version.

// You should have received a copy of the GNU General Public License along
// with this program; if not, write to the Free Software Foundation, Inc., 675
// Mass Ave, Cambridge, MA 02139, USA.

// msgid.cpp
// Routines to handle MSGID generation.
// It can either do independent MSGID generation, or use an IDSERVER.DAT file.

#include "msgid.h"
#include "crc32.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef __EMX__
#include <sys/time.h>
#include <dos.h>
#define INCL_DOSPROCESS
#include <os2.h>
#endif
#ifdef __TURBOC__
#include <time.h>
#include <dos.h>
#endif

// Constants for the IDSERVER.DAT.
#define SERVERVERSION   0
#define SERVERSIGNATURE 0x1a534449

// Class: MsgidStd
// This is the independent MSGID generation.

// constructor
// Initializes the MSGID number to a function of the current system time.
MsgidStd::MsgidStd()
{
#ifdef __EMX__
    // EMX (GNU C++ for OS/2): Get the time of in seconds and milliseconds
    // since 1970-01-01.
    struct timeval tv;
    struct timezone tz;
    gettimeofday(&tv, &tz);
#define SECONDS tv.tv_sec
#define LOWPART (tv.tv_usec / 10)
#endif
#ifdef __TURBOC__
    // Borland Turbo C++: Get the time of day in both seconds since
    // 1970-01-01, and a structure that contains a resolution down to
    // 100ths of a second.
    struct time t;
    gettime(&t);
    time_t now = time(NULL);
#define SECONDS now
#define LOWPART t.ti_hund
#endif
    // Create the MSGID number.
    msgIdNum = (SECONDS << 4) + (LOWPART & 0x0f);
#undef SECONDS
#undef LOWPART
}

// method: getSerial
// Gets the requested number of MSGID serial numbers.
UINT32 MsgidStd::getSerial(int number)
{
    UINT32 retval = msgIdNum;
    msgIdNum += number;
    return retval;
}

// Class: MsgidServ
// This is the MSGID generation that uses the IDSERVER.DAT file.

// constructor
// Set up the path.
MsgidServ::MsgidServ(const char *name)
{
    strcpy(serverFileName, name);
    if ('\\' != serverFileName[strlen(serverFileName) - 1])
        strcat(serverFileName, "\\");
    strcat(serverFileName, "idserver.dat");
}

// method: getSerial
// Gets the specified amount of MSGID serial numbers.
UINT32 MsgidServ::getSerial(int number)
{
    FILE *serverfile = fopen(serverFileName, "rb+");
    if (NULL == serverfile) {
        // Unable to open. Try creating it instead.
        serverfile = fopen(serverFileName, "wb");
        if (NULL != serverfile) { // We were able to create it, initialize it.
            createfile(serverfile);
        }
        else {
            // Unable to create it, it's probably locked. Try to open it
            // five times.
            unsigned i = 0;
            while (i < 5) {
#ifdef __MSDOS__
                // We have to wait between the calls, so for MS-DOS we call
                // the "release timeslices" function (Int 21, function 1680).
                union REGS r;
                r.x.ax = 0x1680;
                int86(0x21, &r, &r);
                if (0x80 == r.h.al) {
                    // We could not release timeslices (no multitasker is
                    // available), so we do some busy waiting (ugly).
                    float is_never_used;
                    for (unsigned k = 0; k < 65535; k ++)
                        is_never_used = k / 3.0;
                }
                else {
                    // And release a bit more.
                    r.x.ax = 0x1680;
                    int86(0x21, &r, &r);
                }
#endif
#ifdef __EMX__
                // We have to wait between calls, so for OS/2 we call the
                // "sleep" function, and sleeps for a second.
                DosSleep(1000);
#endif
                // Try to open it again.
                serverfile = fopen(serverFileName, "rb+");
                i = serverfile ? 10 : i + 1; // Flag if we could open it.
            }
            if (10 != i) {
                // Failed opening the file. Return a random number instead.
                // (ugly)
#ifdef __TURBOC__
                return random(0xffff) << 16 | random(0xfff);
#endif
#ifdef __EMX__
                return random();
#endif
            }
        }
    }
    
    // Okay, now the file is open.
    fseek(serverfile, 0, SEEK_SET);
    idServerDat serverdat;
    fread(&serverdat, sizeof(idServerDat), 1, serverfile);

    // Check the signature. If it's wrong, reinitialize the file.
    if (SERVERSIGNATURE != serverdat.signature) { // Wrong signature.
        fseek(serverfile, 0, SEEK_SET);
        createfile(serverfile);
    }

    // If there is a CRC32 value, check if it's correct.
    // (I wonder why you really need a CRC32 value on this kind of file...?)
    if (SERVERSIGNATURE != serverdat.crc32) {
        UINT32 crc = 0xffffffff;
        for (UINT8 *byte_p = (UINT8 *) &serverdat;
             byte_p < (UINT8 *) &serverdat.crc32; byte_p ++)
            crc = UPDC32(*byte_p, crc);
        if (serverdat.crc32 != crc) // CRC error
            createfile(serverfile);
    }

    // Get a serial number from the file
    UINT32 retval = serverdat.nextserial;
    // and update the number in the file.
    serverdat.nextserial += number;
    
    // Calculate a new CRC32 value
    UINT32 crc = 0xffffffff;
    for (UINT8 *byte_p = (UINT8 *) &serverdat;
         byte_p < (UINT8 *) &serverdat.crc32; byte_p ++)
        crc = UPDC32(*byte_p, crc);
    // and write it.
    serverdat.crc32 = crc;
    fseek(serverfile, 0, SEEK_SET);
    fwrite(&serverdat, sizeof(idServerDat), 1, serverfile);

    // Close the file.
    fclose(serverfile);

    // Done!
    return retval;
}

// method: createfile (private)
// Creates a new idserver file.
void MsgidServ::createfile(FILE *server)
{
    idServerDat serverdat;

    // Reset the struct.
    memset(&serverdat, 0, sizeof(idServerDat));

    // Put in the data. It's unnessecary to CRC32 it now.
    serverdat.revision = SERVERVERSION;
    serverdat.crc32 = serverdat.signature = SERVERSIGNATURE;

    // Initialize the MSGID value.
    serverdat.nextserial = serverdat.initserial = ((UINT32) time(NULL)) << 5;

    // Write the file.
    fseek(server, 0, SEEK_SET);
    fwrite(&serverdat, sizeof(idServerDat), 1, server);
}
