// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or (at your option)
// any later version.

// You should have received a copy of the GNU General Public License along
// with this program; if not, write to the Free Software Foundation, Inc., 675
// Mass Ave, Cambridge, MA 02139, USA.

// config.cpp
// Routines for reading Indigo's configuration file.

#include "config.h"
#include "fido.h"
#include "debug.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef __LINUX__
#define stricmp strcasecmp
#endif

enum cfgsection_t { none, global, uplinks, areas };

// Some typedefs to make life easier.
typedef char *charP;
typedef FidoAddress *FidoAddressP;
typedef BlueWaveArea *BlueWaveAreaP;
typedef Uplink *UplinkP;

// Prototypes
void split3(charP start_p, charP &pt2_p, charP &pt3_p);
void split2(charP start_p, charP &pt2_p);
int checkCommandString(char *);

// constructor
// Reads the specified configuration file.
Configuration::Configuration(char *fileName, direction_t direction)
{
    doDEBUG(printf("Configuration::Configuration(\"%s\", diretion_t(%d))\n",
                   fileName, (int) direction));

    // Try to open the configuration file. If we can't do it, exit the
    // program with an errorlevel.
    FILE *cfg;
    if (NULL == (cfg = fopen(fileName, "rt"))) {
        fprintf(stderr, "* Configuration file not found: %s\n", fileName);
        exit(1);
    }

    // Initialize data
    numareas = numuplinks = 0;
    arealist_pp = NULL;
    myAddress_pp = NULL;
    uplink_pp = NULL;
    myName_p = NULL;
    uplinklist_pp = NULL;
    compresscmd_p = decompresscmd_p = base_p = myName_p = NULL;

    // The Origin line is originally empty.
    char *origin = strdup("");

    // Initialize standard strings.
    char netmail[50], badmail[50];
    strcpy(netmail, "NetMail");
    strcpy(badmail, "Bad mail");

    // Set the temporary path from the environment variable TEMP.
    if (NULL != (temppath_p = getenv("TEMP")))
        temppath_p = strdup(temppath_p); // Reallocate it.
    else
        temppath_p = strdup("."); // Use the current directory.

    // Set up the Blue Wave path default value.
    bwpath_p = strdup(".");

    // Now read the configuration.
    char configline[256];
    char outboundpath[256] = "";
    cfgsection_t section = none;
    unsigned areacounter = 0, uplinkcounter = 0, defaultzone = 0;
    char *ch_p;
    while (NULL != fgets(configline, sizeof(configline), cfg)) {
        // For each line.

        // Remove the trailing newline character.
        configline[strlen(configline) - 1] = 0;
        doDEBUG(printf("%d \"%s\"", (int) section, configline));

        // Does the current line indicate a section change?
        if (!stricmp(configline, "[GLOBAL]"))
            section = global;
        else if (!stricmp(configline, "[UPLINKS]"))
            section = uplinks;
        else if (!stricmp(configline, "[AREAS]"))
            section = areas;

        // This checks whether the current line is on the form
        // keyword=value[,value...]
        if (section != none && NULL != (ch_p = strchr(configline, '=')) &&
            ';' != configline[0]) {
            *ch_p = 0;      // Split the string at the '=' sign.
            ch_p ++;        // ch_p points to the next part.

            doDEBUG(printf(" => \"%s\"-\"%s\"", configline, ch_p));

            switch (section) {
            case global: // Global settings.
                if (!stricmp(configline, "NAME")) {
                    // "Name=Your name"
                    if (NULL != myName_p)
                        delete myName_p; // Deallocate previous definition.
                    myName_p = strdup(ch_p);
                }
                else if (!stricmp(configline, "NETMAIL")) {
                    // "Netmail=Netmail area name"
                    strncpy(netmail, ch_p, sizeof(netmail));
                    // Make sure there is a trailing null character
                    netmail[sizeof(netmail) - 1] = 0;
                }
                else if (!stricmp(configline, "BADMAIL")) {
                    // "Badmail=Badmail area name"
                    strncpy(badmail, ch_p, sizeof(badmail));
                    // Make sure there is a trailing null character.
                    badmail[sizeof(badmail) - 1] = 0;
                }
                else if (!stricmp(configline, "BASENAME")) {
                    // "Basename=Blue Wave name"
                    if (NULL != base_p)
                        delete base_p; // Deallocate previous definition.
                    base_p = strdup(ch_p);
                }
                else if (!stricmp(configline, "OUTBOUND")) {
                    // "Outbound=Outbound path,default zone"
                    char *ch2_p;
                    split2(ch_p, ch2_p); // Split the data value.
                    strncpy(outboundpath, ch_p, sizeof(outboundpath));
                    // Make sure there is a trailing null character.
                    outboundpath[sizeof(outboundpath) - 1] = 0;
                    // Get the default zone number.
                    sscanf(ch2_p, "%u", &defaultzone);
                }
                else if (!stricmp(configline, "ORIGIN")) {
                    // "Origin=Origin string"
                    free(origin); // Deallocate previous definition.
                    origin = strdup(ch_p);
                }
                else if (!stricmp(configline, "COMPRESS")) {
                    // "Compress=Compression command specification"
                    // Make sure it's a valid string.
                    if (checkCommandString(ch_p)) {
                        // The compression command is ok.
                        if (NULL != compresscmd_p)
                            delete compresscmd_p; // Deallocate previous def.
                        compresscmd_p = strdup(ch_p);
                    }
                    else {
                        // The compression command is not ok.
                        // This is a fatal error.
                        fprintf(stderr, "* Illegal compress command: %s\n",
                                ch_p);
                        exit(1);
                    }
                }
                else if (!stricmp(configline, "DECOMPRESS")) {
                    // "Decompress=Decompression command specification"
                    // Make sure it's a valid string.
                    if (checkCommandString(ch_p)) {
                        // The decompression command is ok.
                        if (NULL != decompresscmd_p)
                            delete decompresscmd_p; // Deallocate previous def.
                        decompresscmd_p = strdup(ch_p);
                    }
                    else {
                        // The decompression command is not ok.
                        // This is a fatal error.
                        fprintf(stderr, "* Illegal decompress command: %s\n",
                                ch_p);
                        exit(1);
                    }
                }
                else if (!stricmp(configline, "TEMPPATH")) {
                    // "TempPath=Path to temporary directory"
                    if (NULL != temppath_p)
                        delete temppath_p; // Deallocate previous definition.
                    temppath_p = strdup(ch_p);
                }
                else if (!stricmp(configline, "BWPATH")) {
                    // "BWPath=Path to Blue Wave packets"
                    if (NULL != bwpath_p)
                        delete bwpath_p; // Deallocate previous definition.
                    bwpath_p = strdup(ch_p);
                }
                else {
                    // The configuration keyword is unknown.
                    fprintf(stderr, "* Illegal configuration option: %s\n",
                            configline);
                    exit(1);
                }
                break;
                
            case uplinks: // Uplink settings.
                if (0 == numuplinks && !stricmp(configline, "UPLINKS")) {
                    // "Uplinks=Number of uplinks"

                    // Extract the number of uplinks.
                    numuplinks = atoi(ch_p);

                    // Allocate space for the address pointer arrays.
                    myAddress_pp = new FidoAddressP[numuplinks];
                    uplink_pp =    new FidoAddressP[numuplinks];

                    // Fill the address pointer arrays with NULL pointers.
                    for (unsigned i = 0; i < numuplinks; i ++) {
                        myAddress_pp[i] = NULL;
                        uplink_pp[i] = NULL;
                    }

                    // If we are in outbound mode, allocate space for the
                    // uplink pointer array (for the PKT files).
                    if (out == direction) {
                        uplinklist_pp = new UplinkP[numuplinks];
                        // Fill the uplink pointer array with NULL pointers.
                        for (unsigned i = 0; i < numuplinks; i ++)
                            uplinklist_pp[i] = NULL;
                    }
                }
                else if (numuplinks != 0 && uplinkcounter < numuplinks &&
                         !stricmp(configline, "ADDRESS")) {
                    // "Address=Your address,Uplink address,Password"
                    char *ch2_p, *ch3_p;
                    split3(ch_p, ch2_p, ch3_p); // Split the data value
                    if (ch3_p) { // There were three parts.
                        unsigned z, n, f, p = 0; // p=0 allows for 3D addresses

                        // Extract the first address (your own).
                        sscanf(ch_p, "%u:%u/%u.%u", &z, &n, &f, &p);
                        // Allocate space for a "your address" in the pointer
                        // array.
                        myAddress_pp[uplinkcounter] =
                            new FidoAddress(z, n, f, p);

                        // Extract the second address (uplink).
                        p = 0; // p=0 allows for 3D addresses.
                        sscanf(ch2_p, "%u:%u/%u.%u", &z, &n, &f, &p);
                        // Allocate space for an uplink address in the pointer
                        // array.
                        uplink_pp[uplinkcounter] =
                            new FidoAddress(z, n, f, p);

                        // If we are in outbound direction, we need to
                        // allocate space for a PKT object ("uplink").
                        if (out == direction) {
                            // Check that we have a outbound path
                            // defined, but continue even if there is
                            // none.
                            if (0 == outboundpath[0]) {
                                puts("* Config error: OUTBOUND must "
                                     "be defined before ADDRESS");
                            }

                            // Create the uplink object.
                            uplinklist_pp[uplinkcounter] =
                                new Uplink(*myAddress_pp[uplinkcounter],
                                           *uplink_pp[uplinkcounter],
                                           outboundpath,
                                           ch3_p, // password
                                           defaultzone,
                                           origin);
                        }

                        uplinkcounter ++;
                    }
                }
                else {
                    // The configuration keyword is unknown.
                    fprintf(stderr, "* Illegal uplink configuration option: %s\n",
                            configline);
                    exit(1);
                }
                break;
                
            case areas: // settings.
                if (0 == numareas && !stricmp(configline, "AREAS")) {
                    // "Areas=Number of areas"

                    // Extract the number of defined echomail areas.
                    sscanf(ch_p, "%u", &numareas);

                    // Add to it the number of non-echomail areas
                    // (one netmail per uplink, plus a badmail area).
                    numareas += uplinkcounter + 1;

                    // In incoming mode, allocate a Blue Wave area pointer
                    // array.
                    if (in == direction) {
                        arealist_pp = new BlueWaveAreaP[numareas];

                        // Create the non-echomail areas.

                        // First the netmail areas. Their tag is created
                        // on the format "[zone.net.node.point]", and their
                        // area numbers are on the format "NExxx" where
                        // xxx is the number of the uplink in our list.
                        // The area title is the string for "netmail" defined
                        // earlier, followed by the address we use.
                        for (unsigned i = 0; i < uplinkcounter; i ++) {
                            char nettag[21];
                            sprintf(nettag, "[%u.%u.%u.%u]",
                                    myAddress_pp[i]->zone,
                                    myAddress_pp[i]->net,
                                    myAddress_pp[i]->node,
                                    myAddress_pp[i]->point);
                            char netnum[6], nettitle[50];
                            sprintf(netnum, "NE%03u", i);
                            sprintf(nettitle, "%s %u:%u/%u.%u", netmail,
                                    myAddress_pp[i]->zone, myAddress_pp[i]->net,
                                    myAddress_pp[i]->node,
                                    myAddress_pp[i]->point);
                            arealist_pp[i] = new BlueWaveArea(1, netnum, nettag,
                                                              nettitle,
                                                              myAddress_pp[i]->zone);
                        }

                        // Secondly, create the badmail area.
                        arealist_pp[uplinkcounter] =
                            new BlueWaveArea(0, "BAD", "BADMAIL", badmail,
                                             myAddress_pp[0]->zone);
                    }

                    // Remember the number of the badmail area.
                    badareano = uplinkcounter;
                    // Set the area counter to point past the areas
                    // created here.
                    areacounter = uplinkcounter + 1;
                }
                else if (areacounter < numareas) {
                    // "Echotag=Your address,Area number,Area title"
                    char *ch2_p, *ch3_p;
                    split3(ch_p, ch2_p, ch3_p);

                    // configline -> echotag
                    // ch_p       -> address
                    // ch2_p      -> number
                    // ch3_p      -> name

                    if (ch3_p) { // There were three parts.
                        if (in == direction) {
                            // If in inbound mode, we need to allocate a
                            // Blue Wave area object for our area.
                            arealist_pp[areacounter] =
                                new BlueWaveArea(0, ch2_p, configline, ch3_p,
                                                 atoi(ch_p));
                        }
                        else {
                            // If in outbound mode, we need to add the area
                            // to the list of known areas for the uplink
                            // that is to be sent to.

                            // Extract the address for the area.
                            unsigned z, n, f, p = 0;
                            sscanf(ch_p, "%u:%u/%u.%u", &z, &n, &f, &p);

                            // Search for the uplink.
                            int uplink = uplinkNumber(FidoAddress(z, n, f, p));

                            if (-1 == uplink) {
                                // If we didn't find an uplink setup,
                                // report this, and consider it a fatal
                                // error. This error is only found in
                                // outbound mode.
                                fprintf(stderr,
                                        "* Area %s: missing uplink for %s\n",
                                        configline, ch_p);
                                exit(1);
                            }
                            else {
                                // Insert the area into the list of known
                                // areas.
                                uplinklist_pp[uplink]->addArea(configline);
                            }
                        }
                        // Increase the area counter.
                        areacounter ++;
                        
                        doDEBUG(printf("  areacounter = %u", areacounter));
                    }
                    else {
                        // This area definition is illegal.
                        fprintf(stderr, "* Illegal area definition: %s\n",
                                configline);
                        exit(1);
                    }
                }
                else {
                    // If we still have areas left, but the number of
                    // areas that we allocated are to few, warn the
                    // user about it, and consider it a fatal error.
                    if (areacounter < numareas) {
                        fputs("* Too few area definitions allocated!\n",
                              stderr);
                        exit(1);
                    }
                }
                break;
            }
        }
        doDEBUG(puts(""));
    }

    fclose(cfg);
    
    // Now we have read the configuration file, check that some criteria
    // are fullfilled.

    // 1: The number of uplinks allocated should match the number of
    // uplinks read. If not, consider it a fatal error.
    if (uplinkcounter < numuplinks) {
        fprintf(stderr, "* %u uplinks allocated, only %u used!\n",
                numuplinks, uplinkcounter);
        exit(1);
    }

    // 2: The number of areas allocated should match the number of
    // areas read. If not, consider it a fatal error.
    if (areacounter < numareas) {
        fputs("* Too many area definitions allocated!\n", stderr);
        doDEBUG(fprintf(stderr, "areacounter=%u, numareas=%u", areacounter,
                        numareas));
        exit(1);
    }

    // 3: There should be a path to put the Blue Wave packets in, if
    // we are in inbound mode. If not, consider it a fatal error.
    if (NULL == bwpath_p && in == direction) {
        fputs("* BwPath setup missing", stderr);
        exit(1);
    }

    // 4: There should be a Blue Wave base name defined.
    // If not, consider it a fatal error.
    if (NULL == base_p) {
        fputs("* BaseName setup missing", stderr);
        exit(1);
    }

    // 5: There should be a decompression command defined, if we are
    // in outbound mode. If not, consider it a fatal error.
    if (NULL == decompresscmd_p && out == direction) {
        fputs("* Decompression command setup missing", stderr);
        exit(1);
    }

    // 6: There should be a compression command defined, if we are in
    // inbound mode. If not, consider it a fatal error.
    if (NULL == compresscmd_p && in == direction) {
        fputs("* Compression command setup missing", stderr);
        exit(1);
    }

    // Free the origin line allocation. We don't need it anymore.
    free(origin);
}

// destructor
// Deallocate things we have allocated.
Configuration::~Configuration()
{
    doDEBUG(printf("Configuration::~Configuration()\n"));

    // Kill allocated stuff.
    if (NULL != base_p)
        delete base_p;

    if (NULL != myName_p)
        delete myName_p;

    if (NULL != temppath_p)
        delete temppath_p;

    if (NULL != decompresscmd_p)
        delete decompresscmd_p;

    if (NULL != compresscmd_p)
        delete compresscmd_p;

    if (NULL != bwpath_p)
        delete bwpath_p;

    if (NULL != myAddress_pp) {
        for (unsigned i = 0; i < numuplinks; i ++)
            if (NULL != myAddress_pp[i])
                delete myAddress_pp[i];
        delete myAddress_pp;
    }

    if (NULL != uplink_pp) {
        for (unsigned i = 0; i < numuplinks; i ++)
            if (NULL != uplink_pp[i])
                delete uplink_pp[i];
        delete uplink_pp;
    }

    // Kill the area list
    if (NULL != arealist_pp) {
        for (unsigned i = 0; i < numareas; i ++)
            if (NULL != arealist_pp[i])
                delete arealist_pp[i];
        delete arealist_pp;
    }

    // Kill the uplink list
    if (NULL != uplinklist_pp) {
        for (unsigned i = 0; i < numuplinks; i ++)
            if (NULL != uplinklist_pp[i])
                delete uplinklist_pp[i];
        delete uplinklist_pp;
    }
}

// method: uplinkNumber
// Find a uplink number in the configuration that matches the address given.
int Configuration::uplinkNumber(FidoAddress &findMe)
{
    doDEBUG(printf("Configuration::uplinkNumber(%u:%u/%u.%u) => ",
            findMe.zone, findMe.net, findMe.node, findMe.point));

    // Search the list, looking at both the uplink and own addresses.
    for (unsigned i = 0; i < numuplinks; i ++)
        if (findMe == *uplink_pp[i] || findMe == *myAddress_pp[i]) {
            doDEBUG(printf("%d\n", i));
            return i; // match!
    }

    doDEBUG(puts("Missing!"));
    return -1;  // No match.
}

// method: command
// Creates a [de]compression command line.
char *Configuration::command(char *format, char *str1, char *str2)
{
    doDEBUG(printf("Configuration::command(\"%s\", \"%s\", \"%s\")\n",
                   format, str1, str2));
    static char cmdline[256];

    // If we don't have a template to build on, then don't try.
    if (NULL == format)
        return NULL;

    // Create the command line, and return it to the caller.
    sprintf(cmdline, format, str1, str2);
    return cmdline;
}

// method: tempName
// Create a temporary file name in the defined temporary directory.
char *Configuration::tempName(char *tempFileName)
{
    doDEBUG(printf("Configuration::tempName(\"%s\")\n", tempFileName));
    static char tempname[256];

    // Copy over the temporary path.
    strcpy(tempname, temppath_p);

    // Add a trailing backslash if there is none.
    if ('\\' != tempname[strlen(tempname) - 1])
        strcat(tempname, "\\");

    // Add the file name as specified.
    if (tempFileName)
        strcat(tempname, tempFileName);

    // And return the result.
    return tempname;
}

// method: bwName
// Create a Blue Wave packet name, based on the number given.
char *Configuration::bwName(int number)
{
    doDEBUG(printf("Configuration::bwName(%d)\n", number));

    // Since we are dealing with three-number extensions here, make sure
    // the number isn't bigger than 999, or less than zero.
    if (number > 999)
        number = 999;

    if (number < 0)
        number = 0;

    // Move it to a string.
    char ext[4];
    sprintf(ext, "%03d", number);

    // Call the "extension in string" version.
    return bwName(ext);
}

// method: bwName
// Create a Blue Wave packet name, based on the extension string given.
char *Configuration::bwName(char *ext_p)
{
    doDEBUG(printf("Configuration::bwName(\"%s\")\n", ext_p));
    static char bwname[256];

    // If we don't have a base name, don't try to create a BW name.
    if (NULL == base_p)
        return NULL;

    // 'slash' will point either to a backslash or nothing at all.
    char *slash = "\\";
    if ('\\' == bwpath_p[strlen(bwpath_p) - 1])
        slash ++;

    // Create the name and return it.
    sprintf(bwname, "%s%s%s.%s", bwpath_p, slash, base_p, ext_p);
    return bwname;
}

// split3
// Split a string into three pieces at the comma signs.
void split3(charP start_p, charP &pt2_p, charP &pt3_p)
{
    doDEBUG(printf("\nsplit3(\"%s\", charP &, charP &) => ", start_p));

    if (NULL != (pt2_p = strchr(start_p, ','))) {
        *pt2_p = 0;
        pt2_p ++;
        if (NULL != (pt3_p = strchr(pt2_p, ','))) {
            *pt3_p = 0;
            pt3_p ++;
            doDEBUG(printf("\"%s\" - \"%s\" - \"%s\"\n", start_p, pt2_p,
                    pt3_p));
        }
    }
    else
        pt3_p = NULL;
}

// split2
// Split a string into two pieces at the comma signs.
void split2(charP start_p, charP &pt2_p)
{
    doDEBUG(printf("\nsplit2(\"%s\", charP &) => ", start_p));

    if (NULL != (pt2_p = strchr(start_p, ','))) {
        *pt2_p = 0;
        pt2_p ++;
        doDEBUG(printf("\"%s\" - \"%s\"\n", start_p, pt2_p));
    }
}

// checkCommandString
// Check that a [de]compression command specification has the correct amount
// of parameters.
int checkCommandString(char *cmdstring)
{
    char *tmp;

    if (NULL != (tmp = strchr(cmdstring, '%'))) {
        tmp ++;
        if ('s' == *tmp) { // First token was a %s
            if (NULL != (tmp = strchr(tmp, '%'))) {
                tmp ++;
                if ('s' == *tmp) { // Second token was a %s
                    tmp ++;
                    if (NULL == strchr(tmp, '%'))
                        return 1; // No more % signs -> it is ok!
                }
            }
        }
    }

    // If we fall out of any of the ifs, the command string is not ok.
    return 0;
}
