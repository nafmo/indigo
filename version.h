// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or (at your option)
// any later version.

// You should have received a copy of the GNU General Public License along
// with this program; if not, write to the Free Software Foundation, Inc., 675
// Mass Ave, Cambridge, MA 02139, USA.

// version.h
// Header file containing version number and program name in various formats.

#ifndef __VERSION_H
#define __VERSION_H

// Revision numbers as used in PKT files. These are generally written in the
// format %d.%02d
#define MAJORREV  0
#define MINORREV  1

// Version number string.
#define VERSTRING "0.01"

// Program title. The name should be "Title/OS", where OS is a short
// string defining the operating system, like "DOS", "OS2", "Linux",
// "NT".
#ifdef __MSDOS__
#define PROGNAME "Indigo/DOS"
#endif
#ifdef __EMX__
#define PROGNAME "Indigo/OS2"
#endif
#ifdef __LINUX__
#define PROGNAME "Indigo/Linux"
#endif

// If the compiler didn't get cought by the ifdefs above, report it as a
// compiler error.
#ifndef PROGNAME
#error PROGNAME was not defined
#endif

#endif
