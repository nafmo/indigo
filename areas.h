// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or (at your option)
// any later version.

// You should have received a copy of the GNU General Public License along
// with this program; if not, write to the Free Software Foundation, Inc., 675
// Mass Ave, Cambridge, MA 02139, USA.

// areas.h
// Header for the Blue Wave area class.
#ifndef __AREAS_H
#define __AREAS_H

#include <stdio.h>
#include "bluewave.h"
#include "fido.h"

// Class: BlueWaveArea
// Stores data and messages for one area in a Blue Wave packet.
class BlueWaveArea {
public:
    BlueWaveArea(unsigned isNetmail, char *areaNumber, char *areaTag,
                 char *areaName, unsigned defZone); // constructor
    ~BlueWaveArea();                                // destructor
    void addMessage(char *mFrom, char *mTo, char *mSubject, char *mDate,
                    FidoAddress fromAddr, FILE *input, unsigned isPersonal);
                                     // adds a message to a Blue Wave packet.
    void addToFiles(FILE *infFile, FILE *mixFile, FILE *ftiFile, FILE
                    *datFile);       // adds data from temporary files.
    int isThisYou(const char *);     // check for echotag.
protected:
    INF_AREA_INFO       areainfo;    // record for .inf file.
    MIX_REC             mixrec;      // record for .mix file.
    unsigned            defaultZone; // default zone number.
private:
    char                tempFtiFile[16]; // name of temporary .fti file.
    char                tempDatFile[16]; // name of temporary .dat file.
    char                *myTag;          // this area's echotag.
    unsigned            iAmNetmail;      // flag to check for netmail.
};

#endif
