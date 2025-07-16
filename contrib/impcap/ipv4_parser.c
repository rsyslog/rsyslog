/* ipv4_parser.c
 *
 * This file contains functions to parse IP headers.
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

struct ipv4_header_s {
    /*#if __BYTE_ORDER == __BIG_ENDIAN
        unsigned char version:4;
        unsigned char ihl:4;
    #else*/
    unsigned char ihl : 4;
    unsigned char version : 4;
    // #endif
    uint8_t service;
    uint16_t totLen;
    uint16_t id;
    uint16_t frag;
    uint8_t ttl;
    uint8_t proto;
    uint16_t hdrChksum;
    uint8_t addrSrc[4];
    uint8_t addrDst[4];
    uint8_t pOptions[];
};

typedef struct ipv4_header_s ipv4_header_t;

/*
 *  This function parses the bytes in the received packet to extract IP metadata.
 *
 *  its parameters are:
 *    - a pointer on the list of bytes representing the packet
 *        the first byte must be the beginning of the IP header
 *    - the size of the list passed as first parameter
 *    - a pointer on a json_object, containing all the metadata recovered so far
 *      this is also where IP metadata will be added
 *
 *  This function returns a structure containing the data unprocessed by this parser
 *  or the ones after (as a list of bytes), and the length of this data.
 */
data_ret_t *ipv4_parse(const uchar *packet, int pktSize, struct json_object *jparent) {
    DBGPRINTF("ipv4_parse\n");
    DBGPRINTF("packet size %d\n", pktSize);

    if (pktSize < 20) { /* too small for IPv4 header + data (header might be longer)*/
        DBGPRINTF("IPv4 packet too small : %d\n", pktSize);
        RETURN_DATA_AFTER(0)
    }

    /* Union to prevent cast from uchar to ipv4_header_t */
    union {
        const uchar *pck;
        ipv4_header_t *hdr;
    } ipv4_header_to_char;

    ipv4_header_to_char.pck = packet;
    ipv4_header_t *ipv4_header = ipv4_header_to_char.hdr;

    char addrSrc[20], addrDst[20];
    uint8_t hdrLen = 4 * ipv4_header->ihl; /* 4 x length in words */

    inet_ntop(AF_INET, (void *)&ipv4_header->addrSrc, addrSrc, 20);
    inet_ntop(AF_INET, (void *)&ipv4_header->addrDst, addrDst, 20);

    json_object_object_add(jparent, "net_dst_ip", json_object_new_string((char *)addrDst));
    json_object_object_add(jparent, "net_src_ip", json_object_new_string((char *)addrSrc));
    json_object_object_add(jparent, "IP_ihl", json_object_new_int(ipv4_header->ihl));
    json_object_object_add(jparent, "net_ttl", json_object_new_int(ipv4_header->ttl));
    json_object_object_add(jparent, "IP_proto", json_object_new_int(ipv4_header->proto));


    return ip_proto_parse(ipv4_header->proto, (packet + hdrLen), (pktSize - hdrLen), jparent);
}
