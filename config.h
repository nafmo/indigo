// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or (at your option)
// any later version.

// You should have received a copy of the GNU General Public License along
// with this program; if not, write to the Free Software Foundation, Inc., 675
// Mass Ave, Cambridge, MA 02139, USA.

// config.h
// Header file for the configuration extraction routine.

#ifndef __CONFIG_H
#define __CONFIG_H

#include "fido.h"
#include "areas.h"
#include "uplink.h"

enum direction_t { in, out };

// Class: Configuration
// Handles the program's configuration file.
class Configuration {
public:
    Configuration(char *fileName, direction_t); // constructor (reads config)
    ~Configuration();                                     // destructor
    int uplinkNumber(FidoAddress &);                      // get uplink no.
    inline char *deCompressCommand(char *src, char *dest) // get decompress
        { return command(decompresscmd_p, src, dest); };  //  cmd string.
    inline char *compressionCommand(char *dest, char *src)// get compress
        { return command(compresscmd_p, dest, src); };    //  cmd string.
    char *tempName(char *);  // get file name in temporary directory.
    char *bwName(char *);    // get Blue Wave file name based on extension.
    char *bwName(int);       // get Blue Wave file name based on numeric ext.

    FidoAddress     **myAddress_pp, **uplink_pp;          // address lists.
    int             numuplinks, numareas, badareano;      // various
    BlueWaveArea    **arealist_pp;                        // only with IN
    Uplink          **uplinklist_pp;                      // only with OUT
    char            *base_p, *myName_p, *bwpath_p;        // paths
protected:
    char *command(char *, char *, char *);                // get command.

    char            *decompresscmd_p, *compresscmd_p, *temppath_p; // paths
};

#endif
