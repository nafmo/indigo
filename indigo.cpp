// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or (at your option)
// any later version.

// You should have received a copy of the GNU General Public License along
// with this program; if not, write to the Free Software Foundation, Inc., 675
// Mass Ave, Cambridge, MA 02139, USA.

// indigo.cpp
// Main routines for Indigo.

#include <stdio.h>
#include <string.h>
#include <dos.h>
#include <time.h>
#include <stdlib.h>
#include "areas.h"
#include "pkthead.h"
#include "bluewave.h"
#include "debug.h"
#include "config.h"
#include "version.h"

#ifdef __TURBOC__
#include <dir.h>
#endif
#ifdef __EMX__
#include <process.h>
#include <unistd.h>
#include <emx/syscalls.h>
#endif

#ifdef __LINUX__
#define stricmp strcasecmp
#endif

// Since the file finding routines are different between the compilers, we
// need some macros to get it working.

#ifdef __TURBOC__
#define FINDTYPE struct ffblk
#define FINDFIRST(p,t,a) findfirst(p,t,a)
#define FINDNEXT(t) findnext(t)
#define ARCH_OR_READONLY (FA_ARCH | FA_RDONLY)
#define ALL_FILES (FA_RDONLY | FA_HIDDEN | FA_SYSTEM | FA_DIREC | FA_ARCH)
#define GETFILENAME(t) (t.ff_name)
#endif
#ifdef __EMX__
#define FINDTYPE struct _find
#define FINDFIRST(p,t,a) __findfirst(p,a,t)
#define FINDNEXT(t) __findnext(t)
#define ARCH_OR_READONLY (_A_ARCH | _A_RDONLY)
#define ALL_FILES (_A_RDONLY | _A_HIDDEN | _A_SYSTEM | _A_SUBDIR | _A_ARCH)
#define GETFILENAME(t) (t.name)
#endif

// Prototypes. Never leave home without them.

int pkt2bw(char **, char *, int);
int bw2pkt(char *, char *);
void read2null(FILE *, char *);
int nextFree(char *);

// main
// Checks the command line parameters, and calls the appropriate functions
int main(int argc, char *argv[])
{
    int rc = 0;

    // Show a banner
    puts(PROGNAME " " VERSTRING);

#ifdef __EMX__
    // If we are running the 32-bit protected mode version under DOS,
    // instead run the 16-bit real mode DOS version. This is just because I
    // never bothered to DOS "fix" the protected mode version of the
    // IDSERVER routines.

    if (OS2_MODE != _osmode) {
        spawnv(P_OVERLAY, "indigo.exe", argv);
        return 0;
    }
#endif

    // Copyright info.

    puts("(c) Copyright 1997 Peter Karlsson");
    puts("This program is released under the GNU public license"
#ifdef __MSDOS__
         " except for");
    puts("the SPAWNO routines by Ralf Brown, used to minimize memory while");
    puts("shelling to DOS and running other programs.");
#else
        );
#endif

    puts("You should have received a copy of the GNU General Public License along");
    puts("with this program; if not, write to the Free Software Foundation, Inc., 675");
    puts("Mass Ave, Cambridge, MA 02139, USA.");

    // Initialize.

    tzset();

    // Check the command line parameters.

    if (argc < 3) { // Less than three are too few.
        // Extract the program name from the path in argv[0].
        char *ch_p = strrchr(argv[0], '\\');
        ch_p = ch_p ? ch_p + 1 : argv[0];

        fprintf(stderr, "Usage: %s [-i[drive:][path]configfile] IN pktfile[s]\n"
                        "       %s [-i[drive:][path]configfile] OUT newfile\n", ch_p, ch_p);
        rc = 0;
    }
    else { // Enough parameters
        unsigned paramoffset = 0;

        // Do we have a configuration file parameter?

        char *configfile;
        if (!strncmp("-i", argv[1], 2)) {
            configfile = strdup(&argv[1][2]);
            paramoffset = 1;
        }
        else
            configfile = "indigo.cfg";

        // Check direction (IN/OUT).

        if (!stricmp("IN", argv[1 + paramoffset]))
            rc = pkt2bw(&argv[2 + paramoffset], configfile,
                        argc - 2 - paramoffset);
        else if (!stricmp("OUT", argv[1 + paramoffset])) {
            if (3 + paramoffset == argc)
                rc = bw2pkt(argv[2 + paramoffset], configfile);
            else {
                fprintf(stderr, "Illegal number of parameters to OUT: %d",
                        argc - 2);
                rc = 1;
            }
        }
        else {
            fprintf(stderr, "Illegal direction parameter: %s\n", argv[1]);
            rc = 1;
        }
        if (paramoffset)
            delete configfile;
    }

    return rc;
}

// pkt2bw
// This routine converts a Fidonet PKT file into a Blue Wave packet.
int pkt2bw(char **pktnames, char *paramfile, int numpars)
{
    doDEBUG(printf("pkt2bw((char *[]) %p -> \"%s\"..., \"%s\", %d)\n",
            (void *) pktnames, pktnames[0], paramfile, numpars));

    // Read the configuration file
    Configuration *configdata_p = new Configuration(paramfile, in);
    if (0 == configdata_p->numuplinks || 0 == configdata_p->numareas)
        return 1;

    // Read all the PKT files, one at a time
    // -*-Todo: process these in chronological order.-*-
    unsigned i = 0;
    while (numpars --) {
        doDEBUG(printf("Searching PKT files for: %s\n", pktnames[i]));

        // Extract the path to the PKT file spec.
        // findfirst/findnext only returns the actual name, so we need to keep
        // it.

        char *searchpath = strdup(pktnames[i]);
        char *ch_p;
        if (NULL != (ch_p = strrchr(searchpath, '\\')))
            *(ch_p + 1) = 0; // path up to the backslash
        else
            *searchpath = 0; // no path was given

        // Now look for all PKT files that are matching the file specification

        FINDTYPE ffblk;
        int done = FINDFIRST(pktnames[i], &ffblk, ARCH_OR_READONLY);
        while (!done) {
            char *pkt = new char[strlen(searchpath) + strlen(GETFILENAME(ffblk)) + 1];
            strcpy(pkt, searchpath);
            strcat(pkt, GETFILENAME(ffblk));
            doDEBUG(printf("Found PKT file: %s\n", pkt));

            // Open the PKT file
            FILE *pktf;
            if (NULL == (pktf = fopen(pkt, "rb")))
                return 1; // Perhaps aborting here is a Bad Idea?
            fseek(pktf, 0, SEEK_END);
            long pktsize = ftell(pktf);
            fseek(pktf, 0, SEEK_SET);

            // Read the PKT file, and add the messages to the Blue Wave packet
            pktheader_t pktheader;

            // Get the PKT header.
            fread(&pktheader, sizeof(pktheader_t), 1, pktf);

            // Figure out who this is from.
            int thisuplinknumber =
                configdata_p->uplinkNumber(FidoAddress(pktheader.orgzone,
                                                       pktheader.orgnet,
                                                       pktheader.orgnode,
                                                       pktheader.orgpoint));

            // If we didn't find a matching uplink, report it, and proceed as
            // if the uplink matched number one.
            if (-1 == thisuplinknumber) {
                fprintf(stderr, "* Undefined uplink: %u:%u/%u.%u\n",
                        pktheader.orgzone, pktheader.orgnet, pktheader.orgnode,
                        pktheader.orgpoint);
                thisuplinknumber = 0;
            }

            // Now read each message.
            pktmsg_t msgheader;
            do {
                ifDEBUG(printf("%s [%ld/%ld byte]\n", pkt, ftell(pktf), pktsize),
                        printf("%s (%06.2f%%)\r", pkt, (100.0 * ftell(pktf)) / pktsize));
                // Fetch the message header.
                msgheader.pktver = 0;
                fread(&msgheader, sizeof(pktmsg_t), 1, pktf);
                // Run if we get indication of version 2 (=message)
                if (2 == msgheader.pktver) {
                    BlueWaveArea *thisarea_p;

                    // Fetch recipient, sender, and subject
                    char mFrom[36], mTo[36], mSubj[72];
                    read2null(pktf, mTo);
                    read2null(pktf, mFrom);
                    read2null(pktf, mSubj);

                    // Fetch AREA kludge, if any
                    long textpos = ftell(pktf);
                    char area[256];
                    area[255] = 0;
                    for (unsigned j = 0; j < 255; j ++) {
                        area[j] = fgetc(pktf);
                        if (13 == area[j]) {
                            area[j] = 0;
                            break;
                        }
                    }
                    if (area == strstr(area, "AREA:")) { // AREA found
                        // Match the area tag
                        thisarea_p = NULL;
                        for (unsigned j = 0; j < configdata_p->numareas &&
                             NULL == thisarea_p; j ++) {
                            if (configdata_p->arealist_pp[j]->isThisYou(&area[5]))
                                thisarea_p = configdata_p->arealist_pp[j];
                        }
                        // We didn't find a matching area.
                        if (!thisarea_p) {
                            fprintf(stderr, "* Area setup missing: %s\n",
                                    &area[5]);
                            // Put it in the "bad messages" area.
                            thisarea_p = configdata_p->arealist_pp[configdata_p->badareano];
                            // Reset file pointer.
                            fseek(pktf, textpos, SEEK_SET);
                        }
                    }
                    else { // This is a netmail area, reset the file pointer.
                        fseek(pktf, textpos, SEEK_SET);
                        thisarea_p = configdata_p->arealist_pp[thisuplinknumber];
                    }

                    // Save the message, if we found an area to put it in.
                    if (thisarea_p) {
                        FidoAddress thisaddr(0, msgheader.orgnet, msgheader.orgnode, 0);
                        thisarea_p->addMessage(mFrom, mTo, mSubj,
                                               (char *) &msgheader.datetime[0],
                                               thisaddr, pktf,
                                               0 == stricmp(mTo, configdata_p->myName_p));
                    }
                    else { // We should never get here.
                        doDEBUG(puts("Skipping msg (no matching area)"));
                        int ch;
                        while (0 != (ch = fgetc(pktf)) && EOF != ch)
                            ; // null
                    }
                }
            } while (2 == msgheader.pktver);
            ifDEBUG(,printf("%s (done)   \n", pkt));

            // Close the PKT file.
            fclose(pktf);

            // And remove it.
            remove(pkt);

            done = FINDNEXT(&ffblk);
        } // while

        // -*-Todo: Under OS/2, we ought to close the findfirst handle,
        // but I couldn't find any findclose function in the EMX runtime
        // library.-*-
        // FINDCLOSE(&ffblk);

        i ++;
        delete searchpath;
    } // while

    // Create the Blue Wave files.
    FILE *inf, *mix, *fti, *dat;
    char nametemplate[256];
    strcpy(nametemplate, configdata_p->tempName(configdata_p->base_p));
    strcat(nametemplate, ".%s");

    char filename[256];

    // .inf file (packet and area information)
    sprintf(filename, nametemplate, "inf");
    inf = fopen(filename, "wb");

    // .mix file (message area index)
    sprintf(filename, nametemplate, "mix");
    mix = fopen(filename, "wb");

    // .fti file (message index)
    sprintf(filename, nametemplate, "fti");
    fti = fopen(filename, "wb");

    // .dat file (message texts)
    sprintf(filename, nametemplate, "dat");
    dat = fopen(filename, "wb");

    // Check if we could open the files.
    if (NULL == inf || NULL == mix || NULL == fti || NULL == dat) {
        fputs("* Unable to create output file(s)", stderr);
        delete configdata_p;
    }

    // Create the header of the .inf file
    INF_HEADER infheader;
    memset(&infheader, 0, sizeof(INF_HEADER));

    infheader.ver = PACKET_LEVEL;
    strcpy((char *) &infheader.loginname[0], configdata_p->myName_p);
    strcpy((char *) &infheader.aliasname[0], configdata_p->myName_p);
    infheader.zone  = configdata_p->myAddress_pp[0]->zone;
    infheader.net   = configdata_p->myAddress_pp[0]->net;
    infheader.node  = configdata_p->myAddress_pp[0]->node;
    infheader.point = configdata_p->myAddress_pp[0]->point;
    strcpy((char *) &infheader.sysop[0], configdata_p->myName_p);
    infheader.ctrl_flags = INF_NO_CONFIG | INF_NO_FREQ;
    sprintf((char *) &infheader.systemname[0], "%u:%u/%u.%u ("
            PROGNAME " " VERSTRING ")",
            configdata_p->myAddress_pp[0]->zone,
            configdata_p->myAddress_pp[0]->net,
            configdata_p->myAddress_pp[0]->node,
            configdata_p->myAddress_pp[0]->point);
    infheader.uflags = INF_EXT_INFO;
    // -*-Todo: We ought to allow more netmail flags, but since there
    // is no handler for these flags written, I thought it was a
    // Good Idea to not do that, yet-*-
//  infheader.netmail_flags = INF_CAN_CRASH | INF_CAN_ATTACH |
//                            INF_CAN_KSENT | INF_CAN_HOLD   |
//                            INF_CAN_IMM   | INF_CAN_FREQ   |
//                            INF_CAN_DIRECT;
    infheader.can_forward = 1; // I find this a stupid restriction.
    infheader.inf_header_len = sizeof(INF_HEADER);
    infheader.inf_areainfo_len = sizeof(INF_AREA_INFO);
    infheader.mix_structlen = sizeof(MIX_REC);
    infheader.fti_structlen = sizeof(FTI_REC);
    infheader.uses_upl_file = 1;
    infheader.from_to_len = 35;
    infheader.subject_len = 71;
    strcpy((char *) &infheader.packet_id[0], configdata_p->base_p);

    // Write the header
    fwrite(&infheader, sizeof(infheader), 1, inf);

    // Let each area write their parts of the files
    for (unsigned j = 0; j < configdata_p->numareas; j ++) {
        configdata_p->arealist_pp[j]->addToFiles(inf, mix, fti, dat);
    }

    // Close them
    fclose(inf);
    fclose(mix);
    fclose(fti);
    fclose(dat);

    // Now we need to create the BW archive.

    // Get a name for the packet.
    char *bwname = configdata_p->bwName(nextFree(configdata_p->bwName("*")));

    // These files are to be included.
    sprintf(filename, nametemplate, "*");

    // Set up the command line.
    char *cmdline = configdata_p->compressionCommand(bwname, filename);
    if (NULL == cmdline) {
        fputs("* No decompression command defined", stderr);
        return 1;
    }

    doDEBUG(printf("Compress commandline is \"%s\"\n", cmdline));

    // Release memory from the configuration.
    delete configdata_p;

    // Call the archiver.
    int rc = system(cmdline);
    doDEBUG(printf("Exitcode => %d\n", rc));
    if (rc) {
        fprintf(stderr, "* Compression command %s returned exit code %d.\n",
                cmdline, rc);
        return 1;
    }

    // Remove the files that have now been archived.
    sprintf(filename, nametemplate, "inf");
    remove(filename);
    sprintf(filename, nametemplate, "mix");
    remove(filename);
    sprintf(filename, nametemplate, "fti");
    remove(filename);
    sprintf(filename, nametemplate, "dat");
    remove(filename);

    // All's well, that ends well.
    return 0;
}

// read2null
// Reads a null terminated string from a file.
void read2null(FILE *input, char *dest_p)
{
    doDEBUG(printf("read2null((FILE *) %p, (char *) %p) => ", (void *) input,
            (void *) dest));

    int ch;
    while (0 != (ch = fgetc(input)) && EOF != ch) {
        *dest_p = ch;
        dest_p ++;
        doDEBUG(fputc(ch, stdout));
    }
    *dest_p = 0; // nullterminate, just to be sure.
    doDEBUG(puts(""));
}

// bw2pkt
// This routine converts a .new archive to an outgoing PKT file.
int bw2pkt(char *newName, char *paramfile)
{
    doDEBUG(printf("bw2pkt(\"%s\", \"%s\")\n", newName, paramfile));

    // Read the configuration file.
    Configuration configdata(paramfile, out);

    // We have to decompress the .new packet first

    // Get the decompression command line.
    char *cmdline = configdata.deCompressCommand(newName, configdata.tempName(NULL));
    if (NULL == cmdline) {
        fputs("* No decompression command defined", stderr);
        return 1;
    }

    doDEBUG(printf("Decompressing with \"%s\"\n", cmdline));

    // And do it.
    int rc = system(cmdline);
    doDEBUG(printf("Exitcode => %d\n", rc));
    if (rc) {
        fprintf(stderr, "* Decompression command %s returned exit code %d.\n",
                cmdline, rc);
        return 1;
    }

    // The .upl file is now decompressed into our temporary directory.
    // Now we need to find out where that is.
    char *uplNameTmp = configdata.tempName(configdata.base_p);
    char *uplName = new char[strlen(uplNameTmp) + 5];
    strcpy(uplName, uplNameTmp);
    strcat(uplName, ".upl");

    doDEBUG(printf("UPL file is %s\n", uplName));

    // Open the .upl file and process all the messages that are listed
    // in it.
    
    FILE *uplFile;
    if (NULL != (uplFile = fopen(uplName, "rb"))) { // We could open it
        UPL_REC upl_rec;
        UPL_HEADER upl_header;
        // Fetch the .upl file header.
        fread(&upl_header, sizeof(upl_header), 1, uplFile);
        if (0 != upl_header.upl_header_len)
            fseek(uplFile, upl_header.upl_header_len, SEEK_SET); // adjust
        // Find out if there is a size difference between the records in
        // the .upl files, and the record we know of (a future revision).
        long recLenDiff = upl_header.upl_rec_len - sizeof(upl_rec);
        if (0 == upl_header.upl_rec_len)
            recLenDiff = 0;

        // Put something on the screen.
        printf("Mail packet created by %s",
               (char *) upl_header.reader_name);

        // Read all the .upl record.
        while (1 == fread(&upl_rec, sizeof(upl_rec), 1, uplFile)) {
            if (recLenDiff)
                fseek(uplFile, recLenDiff, SEEK_CUR); // adjust

            // Match the .upl echotag to, so that we know where to send it
            // and what address to use.
            Uplink *thisuplink_p = NULL;
            for (unsigned i = 0; i < configdata.numuplinks &&
                 NULL == thisuplink_p; i ++) {
                if (configdata.uplinklist_pp[i]->isThisYours((char *) &upl_rec.echotag[0]))
                    thisuplink_p = configdata.uplinklist_pp[i];
            }
            if (NULL == thisuplink_p)
                thisuplink_p = configdata.uplinklist_pp[0]; // Undefined area.

            // Put the message into the PKT file.
            thisuplink_p->addMessage(upl_header, upl_rec,
                                     configdata.tempName(NULL));
        }

        // Close the .upl file
        fclose(uplFile);

        // and remove it.
        remove(uplName);
        
        delete uplName;
    }
    else {
        fprintf(stderr, "* Cannot read %s\n", uplName);
    }

    return 0;
}

// nextFree
// Finds the next free extension number of a given path declaration.
int nextFree(char *filemask)
{
    doDEBUG(printf("nextFree(\"%s\")", filemask));

    // Initialize the "highest found" counter to zero.
    int highest = 0;

    // Locate all files matching the filespec.
    FINDTYPE ffblk;
    int rc = FINDFIRST(filemask, &ffblk, ALL_FILES);
    while (0 == rc) {
        doDEBUG(printf("match: \"%s\" => ", GETFILENAME(ffblk)));
        
        // Find the file extension.
        char *c_p = strrchr(GETFILENAME(ffblk), '.');
        if (c_p) {
            // Convert it to a number, and check if it's the highest yet.
            int thisnum = atoi(c_p + 1);
            doDEBUG(printf("%d => ", thisnum));
            highest = thisnum > highest ? thisnum : highest;
            doDEBUG(printf("highest=%d\n", highest));
        }
        rc = FINDNEXT(&ffblk);
    }

    // -*-Todo: Under OS/2, we ought to close the findfirst handle,
    // but I couldn't find any findclose function in the EMX runtime
    // library.-*-
    // FINDCLOSE(&ffblk);

    return highest + 1;
}
