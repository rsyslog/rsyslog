/* eth_parser.c
 *
 * This file contains functions to parse Ethernet II headers.
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

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpacked"
#pragma GCC diagnostic ignored "-Wattributes"
struct __attribute__((__packed__)) eth_header_s {
    uint8_t addrDst[6];
    uint8_t addrSrc[6];
    uint16_t type;
};

struct __attribute__((__packed__)) vlan_header_s {
    uint8_t addrDst[6];
    uint8_t addrSrc[6];
    uint16_t vlanCode;
    uint16_t vlanTag;
    uint16_t type;
};
#pragma GCC diagnostic pop

typedef struct eth_header_s eth_header_t;
typedef struct vlan_header_s vlan_header_t;


/*
 *  Get an ethernet header type as uint16_t
 *   and return the correspondence as string
 *  NOTE : Only most common types are present, to complete if needed
 */
static const char *eth_type_to_string(uint16_t eth_type) {
    switch (eth_type) {
        case 0x00bb:  // Extreme Networks Discovery Protocol
            return "EDP";
        case 0x0200:  // PUP protocol
            return "PUP";
        case 0x0800:  // IP protocol
            return "IP";
        case 0x0806:  // address resolution protocol
            return "ARP";
        case 0x88a2:  // AoE protocol
            return "AOE";
        case 0x2000:  // Cisco Discovery Protocol
            return "CDP";
        case 0x2004:  // Cisco Dynamic Trunking Protocol
            return "DTP";
        case 0x8035:  // reverse addr resolution protocol
            return "REVARP";
        case 0x8100:  // IEEE 802.1Q VLAN tagging
            return "802.1Q";
        case 0x88a8:  // IEEE 802.1ad
            return "802.1AD";
        case 0x9100:  // Legacy QinQ
            return "QINQ1";
        case 0x9200:  // Legacy QinQ
            return "QINQ2";
        case 0x8137:  // Internetwork Packet Exchange
            return "IPX";
        case 0x86DD:  // IPv6 protocol
            return "IPv6";
        case 0x880B:  // PPP
            return "PPP";
        case 0x8847:  // MPLS
            return "MPLS";
        case 0x8848:  // MPLS Multicast
            return "MPLS_MCAST";
        case 0x8863:  // PPP Over Ethernet Discovery Stage
            return "PPPoE_DISC";
        case 0x8864:  // PPP Over Ethernet Session Stage
            return "PPPoE";
        case 0x88CC:  // Link Layer Discovery Protocol
            return "LLDP";
        case 0x6558:  // Transparent Ethernet Bridging
            return "TEB";
        default:
            return "UNKNOWN";
    }
}


/*
 *  This function parses the bytes in the received packet to extract Ethernet II metadata.
 *
 *  its parameters are:
 *    - a pointer on the list of bytes representing the packet
 *        the first byte must be the beginning of the ETH header
 *    - the size of the list passed as first parameter
 *    - a pointer on a json_object, containing all the metadata recovered so far
 *      this is also where ETH metadata will be added
 *
 *  This function returns a structure containing the data unprocessed by this parser
 *  or the ones after (as a list of bytes), and the length of this data.
 */
data_ret_t *eth_parse(const uchar *packet, int pktSize, struct json_object *jparent) {
    DBGPRINTF("entered eth_parse\n");
    DBGPRINTF("packet size %d\n", pktSize);
    if (pktSize < 14) { /* too short for eth header */
        DBGPRINTF("ETH packet too small : %d\n", pktSize);
        RETURN_DATA_AFTER(0)
    }

    eth_header_t *eth_header = (eth_header_t *)packet;
    char ethMacSrc[20], ethMacDst[20];
    uint8_t hdrLen = 14;

    ether_ntoa_r((struct ether_addr *)eth_header->addrSrc, ethMacSrc);
    ether_ntoa_r((struct ether_addr *)eth_header->addrDst, ethMacDst);

    json_object_object_add(jparent, "ETH_src", json_object_new_string((char *)ethMacSrc));
    json_object_object_add(jparent, "ETH_dst", json_object_new_string((char *)ethMacDst));

    uint16_t ethType = (uint16_t)ntohs(eth_header->type);

    if (ethType == ETHERTYPE_VLAN) {
        vlan_header_t *vlan_header = (vlan_header_t *)packet;
        json_object_object_add(jparent, "ETH_tag", json_object_new_int(ntohs(vlan_header->vlanTag)));
        ethType = (uint16_t)ntohs(vlan_header->type);
        hdrLen += 4;
    }

    data_ret_t *ret;

    if (ethType < 1500) {
        /* this is a LLC header */
        json_object_object_add(jparent, "ETH_len", json_object_new_int(ethType));
        ret = llc_parse(packet + hdrLen, pktSize - hdrLen, jparent);

        /* packet has the minimum allowed size, so the remaining data is
         * most likely padding, this should not appear as data, so remove it
         * */
        // TODO this is a quick win, a more elaborate solution would be to check if all data
        //  is indeed zero, but that would take more processing time
        if (pktSize <= 60 && ret->pData != NULL) {
            if (!ret->pData[0]) ret->size = 0;
        }
        return ret;
    }

    json_object_object_add(jparent, "ETH_type", json_object_new_int(ethType));
    json_object_object_add(jparent, "ETH_typestr", json_object_new_string((char *)eth_type_to_string(ethType)));
    ret = eth_proto_parse(ethType, (packet + hdrLen), (pktSize - hdrLen), jparent);

    /* packet has the minimum allowed size, so the remaining data is
     * most likely padding, this should not appear as data, so remove it */
    if (pktSize <= 60 && ret->pData != NULL) {
        if (!ret->pData[0]) ret->size = 0;
    }
    return ret;
}
