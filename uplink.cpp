// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or (at your option)
// any later version.

// You should have received a copy of the GNU General Public License along
// with this program; if not, write to the Free Software Foundation, Inc., 675
// Mass Ave, Cambridge, MA 02139, USA.

// uplink.cpp
// Routines for moving outbound Blue Wave packets to a PKT file.

#include "uplink.h"
#include "version.h"
#include "debug.h"
#include "string.h"
#include "bluewave.h"
#include "pkthead.h"
#include "shorttag.h"
#include "msgid.h"
#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#ifdef __EMX__
#include <unistd.h>
#endif

#ifdef __LINUX__
#define stricmp strcasecmp
#endif

// Class: Uplink
// This is the class for outboundPKT files.

// constructor
// Initialize files.
Uplink::Uplink(FidoAddress myAddress, FidoAddress uplinkAddress,
               const char *outboundPath, const char *password, unsigned defZone,
               const char *originstr)
{
    doDEBUG(printf("Uplink::Uplink(%u:%u/%u.%u, %u:%u/%u.%u, \"%s\", \"%s\", %u)\n",
                   myAddress.zone, myAddress.net, myAddress.node, myAddress.point,
                   uplinkAddress.zone, uplinkAddress.net, uplinkAddress.node, uplinkAddress.point,
                   outboundPath, password, defZone));

    // Addresses.
    me = myAddress;
    uplink = uplinkAddress;

    // Put the netmail area in the list of known areas.
    char nettag[26];
    sprintf(nettag, "[%u.%u.%u.%u]", me.zone, me.net, me.node, me.point);
    tagList_p = new tagList_t;
    tagList_p->truncTag = NULL;
    tagList_p->echoTag = strdup(nettag);
    tagList_p->next = NULL;

    // Create the file name for the outbound PKT file. Binkleystyle.
    strncpy(pktFile, outboundPath, sizeof(pktFile));
    pktFile[sizeof(pktFile) - 1] = 0;
    // Is our uplink in another zone than the default? If so, we need to
    // append the zone number to the name of the outbound directory.
    if (uplink.zone != defZone) {
        // If the directory ends with a "\", remove it.
        if ('\\' == pktFile[strlen(pktFile) - 1])
            pktFile[strlen(pktFile) - 1] = 0;
        // And put the zone number in there.
        char zoneExt[8];
        sprintf(zoneExt, ".%03x\\", uplink.zone);
        strcat(pktFile, zoneExt);
    }
    else { // Make sure that the directory ends in a "\".
        if ('\\' != outboundPath[strlen(outboundPath) - 1])
            strcat(pktFile, "\\");
    }
    
    // Node number in hex is the base name (either for the packet name,
    // or the point directory).
    char nodeNo[9];
    sprintf(nodeNo, "%04x%04x", uplink.net, uplink.node);
    strcat(pktFile, nodeNo);
    
    // If we have a point number, the base name is the point number.
    if (0 != uplink.point) {
        char pointDir[14];
        sprintf(pointDir, ".pnt\\%08x", uplink.point);
        strcat(pktFile, pointDir);
    }
    
    // The packet name ends in ".out".
    strcat(pktFile, ".out");

    doDEBUG(printf("PKT will be created as %s\n", pktFile));

    // Create the PKT file.
    createPKT(password);

    // Save the Origin line.
    strcpy(origin, originstr);

    // MSGID serial number support.
    if (getenv("IDSERVER"))
        msgid_p = new MsgidServ(getenv("IDSERVER"));
    else
        msgid_p = new MsgidStd;
}

// method: createPKT
// Creates a PKT file.
void Uplink::createPKT(const char *password)
{
    doDEBUG(printf("Uplink::createPKT(\"%s\")\n", password));

    // Create the PKT file, and write the header, if necessary.
    FILE *pktf = fopen(pktFile, "ab+");
    fseek(pktf, 0, SEEK_END);
    if (0 == ftell(pktf)) {
        // New file, create the header.
        time_t timer = time(NULL);
        struct tm *currtime_p = localtime(&timer);

        // Put the data in the PKT header record.
        pktheader_t pktheader;
        memset(&pktheader, 0, sizeof(pktheader));
        pktheader.orgnode  = me.node;
        pktheader.dstnode  = uplink.node;
        pktheader.year     = currtime_p->tm_year + 1900;
        if (pktheader.year < 1980) // Just in case we have a year 2000 bug.
            pktheader.year += 100;
        pktheader.month    = currtime_p->tm_mon;
        pktheader.day      = currtime_p->tm_mday;
        pktheader.hour     = currtime_p->tm_hour;
        pktheader.min      = currtime_p->tm_min;
        pktheader.sec      = currtime_p->tm_sec;
        pktheader.pktver   = PKTVER;
        pktheader.orgnet   = me.net;
        pktheader.dstnet   = uplink.net;
        pktheader.prodcodl = NOPRODCODE;
        pktheader.pvmajor  = MAJORREV;
        strncpy((char *) &pktheader.password[0], password,
                sizeof(pktheader.password));
        pktheader.qorgzone = pktheader.orgzone = me.zone;
        pktheader.qdstzone = pktheader.dstzone = uplink.zone;
        pktheader.capvalid = CAPVALID;
        pktheader.prodcodh = 0;
        pktheader.pvminor  = MINORREV;
        pktheader.capword  = CAPWORD;
        pktheader.orgpoint = me.point;
        pktheader.dstpoint = uplink.point;
        fwrite(&pktheader, sizeof(pktheader), 1, pktf);

        // Finish it with two null characters.
        fputc(0, pktf);
        fputc(0, pktf);
    }
    fclose(pktf);
}

// destructor
Uplink::~Uplink()
{
    doDEBUG(printf("Uplink[%s]::~Uplink()\n", pktFile));

    // Deallocate what we have allocated: the echotaglinst
    tagList_t *trav_p = tagList_p, *next_p;
    while (NULL != trav_p) {
        next_p = trav_p->next;
        if (NULL != trav_p->echoTag)
            free(trav_p->echoTag); // allocated string.
        if (NULL != trav_p->truncTag)
            delete trav_p->truncTag;
        delete trav_p;          // allokerad item.
        trav_p = next_p;
    }

    // Deallocat MSGID support.
    delete msgid_p;
}

// method: addMessage
// Adds a message, as given in the .upl record, to the PKT file.
void Uplink::addMessage(UPL_HEADER &upl_header, UPL_REC &upl_rec, char *path)
{
    doDEBUG(printf("Uplink[%s]::addMessage(UPL_HEADER, UPL_REC, \"%s\")\n",
            pktFile, path));

    doDEBUG(printf("[%s] %s: %s\n", upl_rec.echotag, upl_rec.from,
                   upl_rec.subj));

    // If the message is marked as inactive ("deleted" in Blue Wave), we
    // shouldn't do anything with it. And we don't.
    if (upl_rec.msg_attr & UPL_INACTIVE)
        return;

    // Set up the PKT message header.
    pktmsg_t msgheader;
    memset(&msgheader, 0, sizeof(msgheader));
    msgheader.pktver  = PKTVER;
    msgheader.orgnode = me.node;
    msgheader.orgnet  = me.net;
    if (upl_rec.msg_attr & UPL_NETMAIL) {
        // With netmail, the destination address of the message is the
        // recipient's.
        msgheader.dstnode = upl_rec.destnode;
        msgheader.dstnet  = upl_rec.destnet;
    }
    else {
        // With echomail, the destination address of the message is the
        // uplink's.
        msgheader.dstnode = uplink.node;
        msgheader.dstnet  = uplink.net;
    }

    // Get the time from the Blue Wave message (which is in Unix format),
    // and convert it to the format the PKT file format wants.
    time_t msg_unix_date = upl_rec.unix_date;
    struct tm *msgtime_p = localtime(&msg_unix_date);
    strftime((char *) &msgheader.datetime[0], 20, "%d %b %y  %H:%M:%S",
             msgtime_p);

    // -*-Todo: Message attributes-*-

    // We need the name of the file with the message in it. We append it
    // to the temporary directory name.
    char filename[256];
    strcpy(filename, path);
    if ('\\' != filename[strlen(filename) - 1])
        strcat(filename, "\\");
    strcat(filename, (char *) &upl_rec.filename[0]);

    // Open it.
    FILE *msg = fopen(filename, "rt");
    if (NULL != msg) {
        // If we could open the file, now we want to copy the message to
        // the PKT file.
        FILE *pktf = fopen(pktFile, "rb+");
        if (NULL != pktf) {
            // Now the PKT file is opened, seek to the end of the file,
            // and then back two characters (the PKT file ends with two
            // nulls, which mark the end of the PKT).
            fseek(pktf, 0L, SEEK_END);
            fseek(pktf, -2L, SEEK_CUR);

            // Write the PKT message header.
            fwrite(&msgheader, sizeof(msgheader), 1, pktf);

            // Write the recipient's and sender's name, and then the subjet.
            fputs((char *) &upl_rec.to[0], pktf);
            fputc(0, pktf);
            fputs((char *) &upl_rec.from[0], pktf);
            fputc(0, pktf);
            fputs((char *) &upl_rec.subj[0], pktf);
            fputc(0, pktf);

            // Put some kludges in the message.
            if (upl_rec.msg_attr & UPL_NETMAIL) {
                // In netmail, we want an INTL kludge, and possibly
                // TOPT and FMPT kludges.
                fprintf(pktf, "\x01INTL %u:%u/%u %u:%u/%u\x0d",
                        upl_rec.destzone, upl_rec.destnet, upl_rec.destnode,
                        me.zone, me.net, me.node);
                if (upl_rec.destpoint)
                    fprintf(pktf, "\x01TOPT %u\x0d", upl_rec.destpoint);
                if (me.point)
                    fprintf(pktf, "\x01" "FMPT %u\x0d", me.point);
            }
            else {
                // In echomail, we want an AREA kludge.
                if (' ' == upl_rec.echotag[0]) // truncated echotag
                    fprintf(pktf, "AREA:%s\x0d",
                            matchLongTag((char *) &upl_rec.echotag[0]));
                else
                    fprintf(pktf, "AREA:%s\x0d", upl_rec.echotag);
            }

            // Create the MSGID kludge (if we don't have a point address,
            // we don't want a .0 on the end (it confuses some software)).
            if (me.point)
                fprintf(pktf, "\x01MSGID: %u:%u/%u.%u %08lx\x0d",
                        me.zone, me.net, me.node, me.point,
                        msgid_p->getSerial(1));
            else
                fprintf(pktf, "\x01MSGID: %u:%u/%u %08lx\x0d",
                        me.zone, me.net, me.node,
                        msgid_p->getSerial(1));

            // If we have a REPLY kludge in the Blue Wave packet, put it
            // in the PKT file.
            if (!strncmp((char *) &upl_rec.net_dest[0], "REPLY: ", 7))
                fprintf(pktf, "\x01%s\x0d", upl_rec.net_dest);

            // Copy the file, converting line ends to CRs.
            int ch, prevch;
            while (EOF != (ch = fgetc(msg))) {
                if ('\n' == ch)
                    fputc(13, pktf);
                else
                    fputc(ch, pktf);
                prevch = ch;
            }

            // End the message with a CR, if there wasn't one.
            if ('\n' != prevch)
                fputc(13, pktf);

            // Put some kludges at the end of the message.
            if (upl_rec.msg_attr & UPL_NETMAIL) {
                // Netmail:
                // Put in a Via kludge with the mail reader's name in it.
                char via[80];
                strftime(via, sizeof(via), "\x01Via %%u:%%u/%%u.%%u "
                         "@%Y%m%d.%H%M%S %%s %%u.%%02u\x0d",
                         msgtime_p);
                fprintf(pktf, via, me.zone, me.net, me.node, me.point,
                        upl_header.reader_tear, upl_header.reader_major,
                        upl_header.reader_minor);

                // And another Via kludge, with this program's name in it.
                time_t timer = time(NULL);
                struct tm *currtime_p = localtime(&timer);
                strftime(via, sizeof(via), "\x01Via %%u:%%u/%%u.%%u "
                         "@%Y%m%d.%H%M%S " PROGNAME " " VERSTRING "\x0d",
                         currtime_p);
                fprintf(pktf, via, me.zone, me.net, me.node, me.point);
            }
            else {
                // Echomail:
                // Add a tearline, with this program's name and version
                fputs("--- " PROGNAME " " VERSTRING, pktf);
                // and then the name of the mailreader, if there is one.
                if (upl_header.reader_tear[0])
                    fprintf(pktf, "+%s", (char *) upl_header.reader_tear);

                // And an Origin line. If we don't have one, we don't
                // write it (naturally).
                if (origin[0])
                    fprintf(pktf, "\x0d * Origin: %s", origin);
                else
                    fputs("\x0d * Origin:", pktf);
                // We end it with the address (in 3D or 4D form).
                if (me.point)
                    fprintf(pktf, " (%u:%u/%u.%u)\x0d", me.zone, me.net,
                            me.node, me.point);
                else
                    fprintf(pktf, " (%u:%u/%u)\x0d", me.zone, me.net, me.node);

                // Then some SEEN-BY lines. If we're not a point, we add
                // both our and our uplink's address to them, otherwise
                // we only add our own.
                if (!me.point)
                    fprintf(pktf, "SEEN-BY: %u/%u %u/%u\x0d",
                            me.net, me.node, uplink.net, uplink.node);
                else
                    fprintf(pktf, "SEEN-BY: %u/%u\x0d",
                            uplink.net, uplink.node);

                // And finally a PATH line.
                fprintf(pktf, "\x01PATH: %u/%u\x0d", me.net, me.node);
            }

            // Finish up.
            fputc(0, pktf);

            // And indicate that there is no more messages (yet...)
            fputc(0, pktf);
            fputc(0, pktf);

            // Close the files.
            fclose(msg);
            fclose(pktf);
            
            // Delete the reply file.
            remove(filename);
        }
        else {
            fprintf(stderr, "* Unable to write message %s to PKT file %s.\n",
                    filename, pktFile);
        }

    }
    else
        fprintf(stderr, "* Unable to open replyfile: %s\n", filename);
}

// method: addArea
// Adds an area to the list of known areas for this uplink.
void Uplink::addArea(const char *echoTag)
{
    doDEBUG(printf("Uplink[%s]::addArea(\"%s\")\n", pktFile, echoTag));

    // Create an entry for the taglist.
    tagList_t *tagentry_p = new tagList_t;
    tagentry_p->echoTag = strdup(echoTag);

    // Truncate the area name if it is too long.
    if (strlen(echoTag) >= sizeof(((INF_AREA_INFO *) NULL)->echotag)) {
        tagentry_p->truncTag = strdup(makeShortTag(echoTag));
        doDEBUG(printf("!! long area name truncated to \"%s\"\n",
                tagentry_p->truncTag));
    }
    else {
        tagentry_p->truncTag = NULL;
    }

    tagentry_p->next = tagList_p->next;
    tagList_p->next = tagentry_p;       // link as number 2 (after netmail)
}

// method: isThisYours
// Checks if the echotag is one of those which we recognize.
int Uplink::isThisYours(const char *echoTag)
{
    doDEBUG(printf("Uplink[%s]::isThisYours(\"%s\")\n", pktFile, echoTag));

    // Check if we know the area.
    tagList_t *trav_p = tagList_p;
    while (NULL != trav_p) {
        // Check the real tag.
        if (!stricmp(trav_p->echoTag, echoTag))
            return 1;           // match!
        // Check the truncated tag, if any.
        if (NULL != trav_p->truncTag && !stricmp(trav_p->truncTag, echoTag))
            return 1;           // match!
        trav_p = trav_p->next;
    }

    return 0;   // no match
}

// method: matchLongTag
// Gets a long echotag from a truncated one.
char *Uplink::matchLongTag(char *shortTag)
{
    // Locate the truncated tag in the list
    tagList_t *trav_p = tagList_p;
    while (NULL != trav_p) {
        if (NULL != trav_p->truncTag && !stricmp(trav_p->truncTag, shortTag))
            return trav_p->echoTag; // match!
        trav_p = trav_p->next;
    }

    return NULL; // no match
}
