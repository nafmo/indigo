// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or (at your option)
// any later version.

// You should have received a copy of the GNU General Public License along
// with this program; if not, write to the Free Software Foundation, Inc., 675
// Mass Ave, Cambridge, MA 02139, USA.

// uplink.h
// Header file for the uplink (PKT) class.

#ifndef __UPLINK_H
#define __UPLINK_H

#include <stdio.h>
#include "msgid.h"
#include "pkthead.h"
#include "datatyp.h"
#include "fido.h"
#include "bluewave.h"

// This is the taglist as used in some of the routines.
typedef struct tagList_s {
    char             *echoTag,      // real
                     *truncTag;     // truncated
    struct tagList_s *next;
} tagList_t;

// Class: Uplink
// Embodies a PKT file to an uplink.
class Uplink {
public:
    Uplink(FidoAddress myAddress, FidoAddress uplinkAddress,
           const char *outboundPath, const char *password,
           unsigned defZone, const char *origin); // Binkleystyle constructor
    ~Uplink();                                        // destructor
    void addMessage(UPL_HEADER &, UPL_REC &, char *); // add msg from .upl file
    void addArea(const char *echoTag);                // add area to known list
    int isThisYours(const char *);                    // ask if area is known
protected:
    FidoAddress     me, uplink;      // Mine and uplink's addresses.
    MsgidAbs        *msgid_p;        // Pointer to MSGID object.
    void createPKT(const char *);    // Create the PKT header.
private:
    tagList_t       *tagList_p;      // Echotag list.
    char            pktFile[256];    // Name of PKT file.
    char            origin[256];     // Origin line string.
    char *matchLongTag(char *);      // Get the long echotag for a short one.
};

#endif
