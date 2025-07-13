/* ftp_parser.c
 *
 * This file contains functions to parse FTP headers.
 *
 * File begun on 2018-11-13
 *
 * Created by:
 *  - Théo Bertin (theo.bertin@advens.fr)
 *
 * With:
 *  - François Bernard (francois.bernard@isen.yncrea.fr)
 *  - Tianyu Geng (tianyu.geng@isen.yncrea.fr)
 *
 * This file is part of rsyslog.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *       http://www.apache.org/licenses/LICENSE-2.0
 *       -or-
 *       see COPYING.ASL20 in the source distribution
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "config.h"
#include "parsers.h"

static const int ftp_cds[] = {
        100, 110, 120, 125, 150,
        200, 202, 211, 212, 213, 214, 215, 220, 221, 225, 226, 227, 228, 229, 230, 231, 232, 250, 257,
        300, 331, 332, 350,
        400, 421, 425, 426, 430, 434, 450, 451, 452,
        500, 501, 502, 503, 504, 530, 532, 550, 551, 552, 553,
        600, 631, 632, 633,
        10000, 100054, 10060, 10061, 10066, 10068,
        0
};

static const char *ftp_cmds[] = {
        "STOR",
        "TYPE",
        "ABOR",
        "ACCT",
        "ALLO",
        "APPE",
        "CDUP",
        "CWD",
        "DELE",
        "HELP",
        "LIST",
        "MKD",
        "MODE",
        "NLST",
        "NOOP",
        "PASS",
        "PASV",
        "PORT",
        "PWD",
        "QUIT",
        "REIN",
        "REST",
        "RETR",
        "RMD",
        "RNFR",
        "RNTO",
        "SITE",
        "SMNT",
        "STAT",
        "STOU",
        "STRU",
        "SYST",
        "USER",
        NULL
};

/*
 * This function searches for a valid command in the header (from the list defined in ftp_cmds[])
 * and returns either the command or a NULL pointer
*/
static const char *check_Command_ftp(uchar *first_part_packet) {
    DBGPRINTF("in check_Command_ftp\n");
    DBGPRINTF("first_part_packet : '%s' \n", first_part_packet);
    int i = 0;
    for (i = 0; ftp_cmds[i] != NULL; i++) {
        if (strncmp((const char *)first_part_packet, ftp_cmds[i], strlen((const char *)ftp_cmds[i]) + 1) == 0) {
            return ftp_cmds[i];
        }
    }
    return "UNKNOWN";
}

/*
 * This function searches for a valid code in the header (from the list defined in ftp_cds[])
 * and returns either the command or a NULL pointer
*/
static int check_Code_ftp(uchar *first_part_packet) {
    DBGPRINTF("in check_Code_ftp\n");
    DBGPRINTF("first_part_packet : %s \n", first_part_packet);
    int i = 0;
    for (i = 0; ftp_cds[i] != 0; i++) {
        if (strtol((const char *)first_part_packet, NULL, 10) == ftp_cds[i]) {
            return ftp_cds[i];
        }
    }
    return 0;
}

/*
 *  This function parses the bytes in the received packet to extract FTP metadata.
 *
 *  its parameters are:
 *    - a pointer on the list of bytes representing the packet
 *        the first byte must be the beginning of the FTP header
 *    - the size of the list passed as first parameter
 *    - a pointer on a json_object, containing all the metadata recovered so far
 *      this is also where FTP metadata will be added
 *
 *  This function returns a structure containing the data unprocessed by this parser
 *  or the ones after (as a list of bytes), and the length of this data.
*/
data_ret_t *ftp_parse(const uchar *packet, int pktSize, struct json_object *jparent) {
    DBGPRINTF("ftp_parse\n");
    DBGPRINTF("packet size %d\n", pktSize);

    if (pktSize < 5) {  /* too short for ftp packet*/
        RETURN_DATA_AFTER(0)
    }
    uchar *packet2 = (uchar *)malloc(pktSize * sizeof(uchar));

    memcpy(packet2, packet, pktSize); // strtok changes original packet
    uchar *frst_part_ftp;
    frst_part_ftp = (uchar *)strtok((char *)packet2, " "); // Get first part of packet ftp
    strtok(NULL, "\r\n");

    if (frst_part_ftp) {
        int code = check_Code_ftp(frst_part_ftp);
        const char *command = check_Command_ftp(frst_part_ftp);
        if (code != 0) {
            json_object_object_add(jparent, "FTP_response", json_object_new_int(code));
        } else if (command != NULL) {
            json_object_object_add(jparent, "FTP_request", json_object_new_string(command));
        }
    }
    free(packet2);
    RETURN_DATA_AFTER(0)
}
