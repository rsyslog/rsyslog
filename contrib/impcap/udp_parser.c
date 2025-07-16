/* udp_parser.c
 *
 * This file contains functions to parse UDP headers.
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

#define DNS_PORT 53

struct udp_header_s {
    uint16_t srcPort;
    uint16_t dstPort;
    uint16_t totalLength;
    uint16_t checksum;
};

typedef struct udp_header_s udp_header_t;

/*
 *  This function parses the bytes in the received packet to extract UDP metadata.
 *
 *  its parameters are:
 *    - a pointer on the list of bytes representing the packet
 *        the first byte must be the beginning of the UDP header
 *    - the size of the list passed as first parameter
 *    - a pointer on a json_object, containing all the metadata recovered so far
 *      this is also where UDP metadata will be added
 *
 *  This function returns a structure containing the data unprocessed by this parser
 *  or the ones after (as a list of bytes), and the length of this data.
 */
data_ret_t *udp_parse(const uchar *packet, int pktSize, struct json_object *jparent) {
    DBGPRINTF("udp_parse\n");
    DBGPRINTF("packet size %d\n", pktSize);

    if (pktSize < 8) {
        DBGPRINTF("UDP packet too small : %d\n", pktSize);
        RETURN_DATA_AFTER(0)
    }

    /* Union to prevent cast from uchar to udp_header_t */
    union {
        const uchar *pck;
        udp_header_t *hdr;
    } udp_header_to_char;

    udp_header_to_char.pck = packet;
    udp_header_t *udp_header = udp_header_to_char.hdr;

    // Prevent endianness issue
    unsigned short int src_port = ntohs(udp_header->srcPort);
    unsigned short int dst_port = ntohs(udp_header->dstPort);

    json_object_object_add(jparent, "net_src_port", json_object_new_int(src_port));
    json_object_object_add(jparent, "net_dst_port", json_object_new_int(dst_port));
    json_object_object_add(jparent, "UDP_Length", json_object_new_int(ntohs(udp_header->totalLength)));
    json_object_object_add(jparent, "UDP_Checksum", json_object_new_int(ntohs(udp_header->checksum)));

    if (src_port == DNS_PORT || dst_port == DNS_PORT) {
        return dns_parse(packet + sizeof(udp_header_t), pktSize - sizeof(udp_header_t), jparent);
    }

    RETURN_DATA_AFTER(8)
}
