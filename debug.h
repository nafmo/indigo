// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or (at your option)
// any later version.

// You should have received a copy of the GNU General Public License along
// with this program; if not, write to the Free Software Foundation, Inc., 675
// Mass Ave, Cambridge, MA 02139, USA.

// debug.h
// Header file for debugging stuff.
#ifndef __DEBUG_H
#define __DEBUG_H

// doDEBUG(q): run q if in debug mode.
// ifDEBUG(x,y): run x if in debug mode, y otherwise.

#ifdef DEBUG
# define doDEBUG(q)     q
# define ifDEBUG(x,y)   x
#else
# define doDEBUG(q)
# define ifDEBUG(x,y)   y
#endif

#endif
