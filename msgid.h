// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or (at your option)
// any later version.

// You should have received a copy of the GNU General Public License along
// with this program; if not, write to the Free Software Foundation, Inc., 675
// Mass Ave, Cambridge, MA 02139, USA.

// msgid.h
// Header file for the MSGID serial number objects.

#ifndef __MSGID_H
#define __MSGID_H

#include "datatyp.h"
#include <stdio.h>

// Structure definition for the idserver.dat file.

#ifdef __EMX__
#pragma pack(1)
#endif

typedef struct {
    UINT32      signature,
                revision,
                initserial,
                nextserial,
                reserved[59],
                crc32;
} idServerDat;

#ifdef __EMX__
#pragma pack()
#endif

// Class: MsgidAbs
// Abstract superclass for the MSGID routines.
class MsgidAbs {
public:
    virtual UINT32 getSerial(int) = 0; // abstract.
};

// Class: MsgidStd, subclass of MsgidAbs
// Standard MSGID number generation class.
class MsgidStd: public MsgidAbs {
public:
    MsgidStd();                        // constructor.
    virtual UINT32 getSerial(int);     // get serial numbers.
protected:
    UINT32  msgIdNum;                  // msgid number
};

// Class: MsgidServ, subclass of MsgidAbs
// Idserver based MSGID number generation class.
class MsgidServ: public MsgidAbs {
public:
    MsgidServ(const char *);           // constructor (path is argument).
    virtual UINT32 getSerial(int);     // get serial numbers.
protected:
    void createfile(FILE *);           // create a new idserver.dat file.
    char serverFileName[256];          // file name of server.
};

#endif
