// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or (at your option)
// any later version.

// You should have received a copy of the GNU General Public License along
// with this program; if not, write to the Free Software Foundation, Inc., 675
// Mass Ave, Cambridge, MA 02139, USA.

// fido.cpp
// Routines for Fidonet addresses.

#include "fido.h"

// constructor
// Initializes address to zero.
FidoAddress::FidoAddress()
{
    zone = net = node = point = 0;
}

// constructor
// Initializes address to the one given.
FidoAddress::FidoAddress(unsigned z, unsigned n, unsigned f, unsigned p)
    : zone(z), net(n), node(f), point(p)
{
    // null
}

// method: =
// Assignes an address from another address.
void FidoAddress::operator=(FidoAddress &addr)
{
    zone = addr.zone;
    net = addr.net;
    node = addr.node;
    point = addr.point;
}

// method: ==
// Compares two addresses.
int FidoAddress::operator==(FidoAddress &addr)
{
    return zone == addr.zone && net == addr.net && node == addr.node &&
           point == addr.point;
}

// Routine to print address on a stream.
ostream &operator<<(ostream &stream, FidoAddress &addr)
{
    stream << addr.zone << ':' << addr.net << '/' << addr.node
           << '.' << addr.point;
    return stream;
}
