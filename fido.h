// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or (at your option)
// any later version.

// You should have received a copy of the GNU General Public License along
// with this program; if not, write to the Free Software Foundation, Inc., 675
// Mass Ave, Cambridge, MA 02139, USA.

// fido.h
// Header file for the Fidonet address object
#ifndef __FIDO_H
#define __FIDO_H

#include "iostream.h"

// Class: FidoAddress
// Handles a Fidonet address.
class FidoAddress {
public:
    FidoAddress();                                       // constructor
    FidoAddress(unsigned, unsigned, unsigned, unsigned); // constructor
    void operator=(FidoAddress &);                       // assignment
    int operator==(FidoAddress &);                       // comparison
    friend ostream &operator<<(ostream &, FidoAddress &);

    unsigned    zone, net, node, point;                  // data
};

#endif
