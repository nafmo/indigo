// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or (at your option)
// any later version.

// You should have received a copy of the GNU General Public License along
// with this program; if not, write to the Free Software Foundation, Inc., 675
// Mass Ave, Cambridge, MA 02139, USA.

// areas.cpp
// Routines for handling Blue Wave packets.

#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include "areas.h"
#include "fido.h"
#include "shorttag.h"
#include "debug.h"
#ifdef __EMX__
#include <unistd.h>
#endif

// Class: BlueWaveArea
// Description: Contains information about one area in a Blue Wave packet.

// constructor
// Initialize data.
BlueWaveArea::BlueWaveArea(unsigned isNetmail, char *areaNumber, char
                           *areaTag, char *areaName, unsigned defZone)
{
    doDEBUG(printf("BlueWaveArea::BlueWaveArea(%u, \"%s\", \"%s\", \"%s\", %u)\n",
            isNetmail, areaNumber, areaTag, areaName, defZone));

    // Zero out the data structures.
    memset(&areainfo, 0, sizeof(INF_AREA_INFO));
    memset(&mixrec,   0, sizeof(MIX_REC)      );

    // Set the default zone number.
    defaultZone = defZone;

    // Copy information needed for the .inf file record.
    strncpy((char *) &areainfo.areanum[0], areaNumber, sizeof(areainfo.areanum));
    areainfo.areanum[sizeof(areainfo.areanum) - 1] = 0; // null terminated
    strncpy((char *) &areainfo.title[0],   areaName,   sizeof(areainfo.title)  );
    areainfo.title[sizeof(areainfo.title) - 1] = 0; // null terminated

    if (strlen(areaTag) >= sizeof(areainfo.echotag)) {
        // The echotag is too long to fit in a .inf record, so we need to
        // truncate it.
        strcpy((char *) &areainfo.echotag[0], makeShortTag(areaTag));
        doDEBUG(printf("!! long area name truncated to \"%s\"\n",
                areainfo.echotag));
    }
    else {
        // The echotag is short enough.
        strncpy((char *) &areainfo.echotag[0], areaTag, sizeof(areainfo.echotag));
    }

    // Remember the real areatag.
    myTag = strdup(areaTag);

    // Set up some area flags.
    areainfo.network_type = INF_NET_FIDONET;
    areainfo.area_flags = INF_SCANNING | INF_ANY_NAME | INF_ECHO |
                          INF_POST;
    areainfo.area_flags |= isNetmail ? INF_NETMAIL | INF_NO_PUBLIC :
                           INF_NO_PRIVATE;
    iAmNetmail = isNetmail;

    // Copy the information we need for the .mix file.
    strncpy((char *) &mixrec.areanum[0], areaNumber, sizeof(mixrec.areanum));

    // Create temporary files for the .fti and .dat files.
    char tempFileNameBase[8];
    strcpy(tempFileNameBase, areaNumber);

    strcpy(tempFtiFile, tempFileNameBase);
    strcat(tempFtiFile, ".ft$");

    strcpy(tempDatFile, tempFileNameBase);
    strcat(tempDatFile, ".da$");

    // Remove the temporary files if they already existed.
    remove(tempDatFile);
    remove(tempFtiFile);
}

// destructor
BlueWaveArea::~BlueWaveArea()
{
    doDEBUG(printf("BlueWaveArea[%s]::~BlueWaveArea()\n", areainfo.areanum));

    // deallocate anything we have allocated
    if (myTag) delete myTag;
}

// method: addMessage
// Adds a message from the PKT file to our area.
void BlueWaveArea::addMessage(char *mFrom, char *mTo, char *mSubject, char
                              *mDate, FidoAddress fromAddress,
                              FILE *input, unsigned isPersonal)
{
    doDEBUG(printf("BlueWaveArea[%s]::addMessage(\"%s\", \"%s\", \"%s\", "
            "\"%s\", %u:%u/%u.%u, (FILE *) %p, %u)\n",
            areainfo.areanum,
            mFrom, mTo, mSubject, mDate, fromAddress.zone, fromAddress.net,
            fromAddress.node, fromAddress.point,
            (void *) input, isPersonal));

    // Each message needs a .fti record.
    FTI_REC ftirec;
    memset(&ftirec, 0, sizeof(FTI_REC));

    // Increase the counters in the .mix file.
    mixrec.totmsgs ++;
    if (isPersonal)
        mixrec.numpers ++;

    // Make sure the sender's and recipient's name does not only consist
    // of uppercase letters (buggy QWK doors)

    // -*-Todo: This is done twice. It should be a function. I'm just too
    // lazy to write one.-*-
    
    // Sender's name.
    if (strlen(mFrom)) {
        unsigned i = 0;
        while (i < strlen(mFrom) && (isupper(mFrom[i]) || !isalpha(mFrom[i])))
            i ++;
        if (i == strlen(mFrom)) { // It was all uppercase, convert.
            unsigned beginword = 1;
            i = 0;
            while (i < strlen(mFrom)) {
                if (!beginword)
                    mFrom[i] = tolower(mFrom[i]);
                beginword = ' ' == mFrom[i] ? 1 : 0;
                i ++;
            }
        }
    }

    // Recipient's name.
    if (strlen(mTo)) {
        unsigned i = 0;
        while (i < strlen(mTo) && (isupper(mTo[i]) || !isalpha(mTo[i])))
            i ++;
        if (i == strlen(mTo)) { // It was all uppercase, convert.
            unsigned beginword = 1;
            i = 0;
            while (i < strlen(mTo)) {
                if (!beginword)
                    mTo[i] = tolower(mTo[i]);
                beginword = ' ' == mTo[i] ? 1 : 0;
                i ++;
            }
        }
    }

    doDEBUG(printf("[%s] %s: %s\n", areainfo.echotag, mFrom, mSubject));

    // Put in .fti record data.
    strncpy((char *) &ftirec.from[0],    mFrom,    sizeof(ftirec.from)   );
    strncpy((char *) &ftirec.to[0],      mTo,      sizeof(ftirec.to)     );
    strncpy((char *) &ftirec.subject[0], mSubject, sizeof(ftirec.subject));
    strncpy((char *) &ftirec.date[0],    mDate,    sizeof(ftirec.date)   );

    // Open the .dat and .fti [temporary] files for writing.
    FILE *datFile, *ftiFile;
    if (NULL != (datFile = fopen(tempDatFile, "ab"))) {
        if (NULL != (ftiFile = fopen(tempFtiFile, "ab"))) {
            // Move to the end of the .fti and .dat files.
            // This is not strictly necessary, but I do it to be on the
            // safe side.
            fseek(ftiFile, 0, SEEK_END);
            fseek(datFile, 0, SEEK_END);

            // Fill the rest of the .fti record fields.
            ftirec.msgnum = mixrec.totmsgs;
            ftirec.msgptr = ftell(datFile); // Offset in the .dat file.
            if (fromAddress.net != 0) {
                ftirec.orig_zone  = fromAddress.zone ? fromAddress.zone
                                    : defaultZone;
                ftirec.orig_net   = fromAddress.net;
                ftirec.orig_node  = fromAddress.node;
            }
            ftirec.flags = 0;   // -*-Todo: Check this one-*-

            // Copy the message, end at null character in input stream.
            // Start with a space (the Blue Wave format demands this).
            fputc(' ', datFile);
            ftirec.msglength = 1;

            // In netmail we need to read some kludges to find the real
            // address of the sender. Since checking for kludges is slow,
            // this check is not done for echomail.

            // -*-Todo: This should be cleaned up, and splitted, one that
            // looks for the first kludges, and uses them, and goes on to
            // the body when there are no more kludges. This will be
            // necessary when the CHRS kludge support is implemented.-*-
            
            if (iAmNetmail) { // I'm netmail, so do kludge checking
                char kludgebuffer[80];
                int isNewLine = 1, isKludgeLine = 0;
                unsigned i = 0;
                int ch = fgetc(input);
                // Do until null character, or end of file.
                while (0 != ch && EOF != ch) {
                    fputc(ch, datFile);
                    ftirec.msglength ++;
                    if (isKludgeLine && i < 80) { // Insert in kludge buffer.
                        kludgebuffer[i ++] = ch;
                        if (13 == ch) { // End of kludge line.
                            kludgebuffer[i - 1] = 0;
                            doDEBUG(printf("kludgebuffer = \"%s\"\n",
                                           kludgebuffer));
                            // INTL and MSGID kludges helps us to find the
                            // complete address. The Blue Wave packet
                            // format does not have a field for the point
                            // address in the .fti file, so it is not
                            // recovered. The reader will take care of it
                            // if it finds a FMPT kludge, which should be
                            // included in the PKT file anyway.

                            if (!strncmp(kludgebuffer, "INTL ", 5)) {
                                // INTL kludge.
                                char *ch_p = strrchr(kludgebuffer, ' ');
                                sscanf(ch_p + 1, "%u:%u/%u",
                                       &ftirec.orig_zone,
                                       &ftirec.orig_net,
                                       &ftirec.orig_node);
                                doDEBUG(printf("=> %u:%u/%u\n",
                                               ftirec.orig_zone,
                                               ftirec.orig_net,
                                               ftirec.orig_node));
                            }
                            else if (!strncmp(kludgebuffer, "MSGID: ", 7)) {
                                // MSGID kludge.
                                unsigned z, n, f;
                                if (3 == sscanf(&kludgebuffer[7], "%u:%u/%u",
                                                &z, &n, &f)) {
                                    ftirec.orig_zone = z;
                                    ftirec.orig_net = n;
                                    ftirec.orig_node = f;
                                    doDEBUG(printf("=> %u:%u/%u\n",
                                                   ftirec.orig_zone,
                                                   ftirec.orig_net,
                                                   ftirec.orig_node));
                                }
                            }
                            i = 0;
                            isKludgeLine = 0;
                        }
                    }
                    
                    // Check if the new line is a kludge line.
                    if (isNewLine)
                        isKludgeLine = 1 == ch;

                    // Does a new line start?
                    isNewLine = 13 == ch;

                    // Get next character.
                    ch = fgetc(input);
                }
            }
            else { // I'm echomail, so skip kludge checking.
                int ch = fgetc(input);
                // Copy everything.
                while (0 != ch && EOF != ch) {
                    fputc(ch, datFile);
                    ftirec.msglength ++;
                    ch = fgetc(input);
                }
            }

            // Write the .fti record to our temporary .fti file.
            fwrite(&ftirec, sizeof(ftirec), 1, ftiFile);
            fclose(ftiFile);
        }
        fclose(datFile);
    }
}

// method: addToFiles
// Adds the data we have accumulated to the real Blue Wave files.
void BlueWaveArea::addToFiles(FILE *infFile, FILE *mixFile, FILE *ftiFile,
                              FILE *datFile)
{
    doDEBUG(printf("BlueWaveArea[%s]::addToFiles((FILE *) %p, (FILE *) %p, "
            "(FILE *) %p, (FILE *) %p)\n",
            areainfo.areanum,
            infFile, mixFile, ftiFile, datFile));

    // Write a record to the .inf file.
    fwrite(&areainfo, sizeof(areainfo), 1, infFile);

    // Write a record to the .mix file
    mixrec.msghptr = ftell(ftiFile);    // Start position in .fti file.
    fwrite(&mixrec, sizeof(mixrec), 1, mixFile);

    // Copy .fti and .dat files, if they exist.
    FILE *thisFti, *thisDat;
    if (NULL != (thisFti = fopen(tempFtiFile, "rb"))) {
        if (NULL != (thisDat = fopen(tempDatFile, "rb"))) {
            printf("Adding messages in %s.\n", areainfo.title);

            UINT32 totalDatSize = 0;

            // Write records to the .fti file.

            // Offset in the "real" .dat file.
            SINT32 datFileOffset = ftell(datFile);
            for(UINT16 i = 0; i < mixrec.totmsgs; i ++) {
                FTI_REC ftirec;
                fread(&ftirec, sizeof(ftirec), 1, thisFti);
                // Increase our intermediate offset to cope with the
                // real .dat file.
                ftirec.msgptr += datFileOffset;
                fwrite(&ftirec, sizeof(ftirec), 1, ftiFile);
                totalDatSize += ftirec.msglength;
            }

            // Copy the .dat file in blocks of 1 Kbyte each.
            char buffer[1024];
            while (totalDatSize >= sizeof(buffer)) {
                fread( &buffer, sizeof(buffer), 1, thisDat);
                fwrite(&buffer, sizeof(buffer), 1, datFile);
                totalDatSize -= sizeof(buffer);
            }
            if (totalDatSize) {
                fread( &buffer, totalDatSize, 1, thisDat);
                fwrite(&buffer, totalDatSize, 1, datFile);
            }

            // Close our temporary files
            fclose(thisDat);
            fclose(thisFti);

            // and delete them.
            remove(tempDatFile);
            remove(tempFtiFile);
        }
        fclose(thisFti);
    }
}

// method: isThisYou
// Checks if we are the area that are requested.
int BlueWaveArea::isThisYou(const char *echotag) {
    doDEBUG(printf("BlueWaveArea[%s]::isThisYou(\"%s\") [I am \"%s\"]\n",
            areainfo.areanum, echotag, myTag));

    // Is this my echotag?
    return 0 == stricmp(echotag, myTag);
}
