/* arp_parser.c
 *
 * This file contains functions to parse ARP and RARP headers.
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

struct arp_header_s {
    uint16_t hwType;
    uint16_t pType;
    uint8_t hwAddrLen;
    uint8_t pAddrLen;
    uint16_t opCode;
    uint8_t pAddr[];
};

typedef struct arp_header_s arp_header_t;

/*
 *  This function parses the bytes in the received packet to extract ARP metadata.
 *
 *  its parameters are:
 *    - a pointer on the list of bytes representing the packet
 *        the first byte must be the beginning of the ARP header
 *    - the size of the list passed as first parameter
 *    - a pointer on a json_object, containing all the metadata recovered so far
 *      this is also where ARP metadata will be added
 *
 *  This function returns a structure containing the data unprocessed by this parser
 *  or the ones after (as a list of bytes), and the length of this data.
 */
data_ret_t *arp_parse(const uchar *packet, int pktSize, struct json_object *jparent) {
    DBGPRINTF("arp_parse\n");
    DBGPRINTF("packet size %d\n", pktSize);

    if (pktSize < 28) { /* too small for ARP header*/
        DBGPRINTF("ARP packet too small : %d\n", pktSize);
        RETURN_DATA_AFTER(0);
    }

    /* Union to prevent cast from uchar to arp_header_t */
    union {
        const uchar *pck;
        arp_header_t *hdr;
    } arp_header_to_char;

    arp_header_to_char.pck = packet;
    arp_header_t *arp_header = arp_header_to_char.hdr;

    char pAddrSrc[20], pAddrDst[20];

    json_object_object_add(jparent, "ARP_hwType", json_object_new_int(ntohs(arp_header->hwType)));
    json_object_object_add(jparent, "ARP_pType", json_object_new_int(ntohs(arp_header->pType)));
    json_object_object_add(jparent, "ARP_op", json_object_new_int(ntohs(arp_header->opCode)));

    if (ntohs(arp_header->hwType) == 1) { /* ethernet addresses */
        char hwAddrSrc[20], hwAddrDst[20];

        ether_ntoa_r((struct ether_addr *)arp_header->pAddr, hwAddrSrc);
        ether_ntoa_r((struct ether_addr *)(arp_header->pAddr + arp_header->hwAddrLen + arp_header->pAddrLen),
                     hwAddrDst);

        json_object_object_add(jparent, "ARP_hwSrc", json_object_new_string((char *)hwAddrSrc));
        json_object_object_add(jparent, "ARP_hwDst", json_object_new_string((char *)hwAddrDst));
    }

    if (ntohs(arp_header->pType) == ETHERTYPE_IP) {
        inet_ntop(AF_INET, (void *)(arp_header->pAddr + arp_header->hwAddrLen), pAddrSrc, 20);
        inet_ntop(AF_INET, (void *)(arp_header->pAddr + 2 * arp_header->hwAddrLen + arp_header->pAddrLen), pAddrDst,
                  20);

        json_object_object_add(jparent, "ARP_pSrc", json_object_new_string((char *)pAddrSrc));
        json_object_object_add(jparent, "ARP_pDst", json_object_new_string((char *)pAddrDst));
    }

    RETURN_DATA_AFTER(28);
}

/*
 *  This function parses the bytes in the received packet to extract RARP metadata.
 *  This is a copy of ARP handler, as structure is the same but protocol code and name are different
 *
 *  its parameters are:
 *    - a pointer on the list of bytes representing the packet
 *        the first byte must be the beginning of the RARP header
 *    - the size of the list passed as first parameter
 *    - a pointer on a json_object, containing all the metadata recovered so far
 *      this is also where RARP metadata will be added
 *
 *  This function returns a structure containing the data unprocessed by this parser
 *  or the ones after (as a list of bytes), and the length of this data.
 */
data_ret_t *rarp_parse(const uchar *packet, int pktSize, struct json_object *jparent) {
    DBGPRINTF("rarp_parse\n");
    DBGPRINTF("packet size %d\n", pktSize);

    if (pktSize < 28) { /* too small for RARP header*/
        DBGPRINTF("RARP packet too small : %d\n", pktSize);
        RETURN_DATA_AFTER(0);
    }

    /* Union to prevent cast from uchar to arp_header_t */
    union {
        const uchar *pck;
        arp_header_t *hdr;
    } arp_header_to_char;

    arp_header_to_char.pck = packet;
    arp_header_t *rarp_header = arp_header_to_char.hdr;

    char pAddrSrc[20], pAddrDst[20];

    json_object_object_add(jparent, "RARP_hwType", json_object_new_int(ntohs(rarp_header->hwType)));
    json_object_object_add(jparent, "RARP_pType", json_object_new_int(ntohs(rarp_header->pType)));
    json_object_object_add(jparent, "RARP_op", json_object_new_int(ntohs(rarp_header->opCode)));

    if (ntohs(rarp_header->hwType) == 1) { /* ethernet addresses */
        char *hwAddrSrc = ether_ntoa((struct ether_addr *)rarp_header->pAddr);
        char *hwAddrDst =
            ether_ntoa((struct ether_addr *)(rarp_header->pAddr + rarp_header->hwAddrLen + rarp_header->pAddrLen));

        json_object_object_add(jparent, "RARP_hwSrc", json_object_new_string((char *)hwAddrSrc));
        json_object_object_add(jparent, "RARP_hwDst", json_object_new_string((char *)hwAddrDst));
    }

    if (ntohs(rarp_header->pType) == ETHERTYPE_IP) {
        inet_ntop(AF_INET, (void *)(rarp_header->pAddr + rarp_header->hwAddrLen), pAddrSrc, 20);
        inet_ntop(AF_INET, (void *)(rarp_header->pAddr + 2 * rarp_header->hwAddrLen + rarp_header->pAddrLen), pAddrDst,
                  20);

        json_object_object_add(jparent, "RARP_pSrc", json_object_new_string((char *)pAddrSrc));
        json_object_object_add(jparent, "RARP_pDst", json_object_new_string((char *)pAddrDst));
    }

    RETURN_DATA_AFTER(28);
}
