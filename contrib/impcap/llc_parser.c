/* llc_parser.c
 *
 * This file contains functions to parse llc headers.
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

/*
 *  This function parses the bytes in the received packet to extract LLC metadata.
 *
 *  its parameters are:
 *    - a pointer on the list of bytes representing the packet
 *        the first byte must be the beginning of the LLC header
 *    - the size of the list passed as first parameter
 *    - a pointer on a json_object, containing all the metadata recovered so far
 *      this is also where LLC metadata will be added
 *
 *  This function returns a structure containing the data unprocessed by this parser
 *  or the ones after (as a list of bytes), and the length of this data.
*/
data_ret_t *llc_parse(const uchar *packet, int pktSize, struct json_object *jparent) {
    DBGPRINTF("entered llc_parse\n");
    DBGPRINTF("packet size %d\n", pktSize);

    if (pktSize < 3) {  /* too short for llc header */
        DBGPRINTF("LLC packet too small : %d\n", pktSize);
        RETURN_DATA_AFTER(0)
    }

    uint8_t dsapField, dsap, ssapField, ssap;
    uint16_t ctrl;
    uint8_t headerLen;

    dsapField = (uint8_t)packet[0];
    ssapField = (uint8_t)packet[1];
    DBGPRINTF("dsapField : %02X\n", dsapField);
    DBGPRINTF("ssapField : %02X\n", ssapField);

    if (dsapField == 0xff && ssapField == 0xff) {
        /* this is an IPX packet, without LLC */
        return ipx_parse(packet, pktSize, jparent);
    }

    if ((packet[2] & 0x03) == 3) {
        /* U frame: LLC control is 8 bits */
        ctrl = (uint8_t)packet[2];
        headerLen = 3;
    } else {
        /* I and S data frames: LLC control is 16 bits */
        ctrl = ntohs((uint16_t)packet[2]);
        headerLen = 4;
    }

    /* don't take last bit into account */
    dsap = dsapField & 0xfe;
    ssap = ssapField & 0xfe;

    json_object_object_add(jparent, "LLC_dsap", json_object_new_int(dsap));
    json_object_object_add(jparent, "LLC_ssap", json_object_new_int(ssap));
    json_object_object_add(jparent, "LLC_ctrl", json_object_new_int(ctrl));

    if (dsap == 0xaa && ssap == 0xaa && ctrl == 0x03) {
        /* SNAP header */
        uint32_t orgCode = packet[headerLen] << 16 |
                           packet[headerLen + 1] << 8 |
                           packet[headerLen + 2];
        uint16_t ethType = packet[headerLen + 3] << 8 |
                           packet[headerLen + 4];
        json_object_object_add(jparent, "SNAP_oui", json_object_new_int(orgCode));
        json_object_object_add(jparent, "SNAP_ethType", json_object_new_int(ethType));
        return eth_proto_parse(ethType, packet + headerLen, pktSize - headerLen, jparent);
    }
    if (dsap == 0x06 && ssap == 0x06 && ctrl == 0x03) {
        /* IPv4 header */
        return ipv4_parse(packet + headerLen, pktSize - headerLen, jparent);
    }
    if (dsap == 0xe0 && ssap == 0xe0 && ctrl == 0x03) {
        /* IPX packet with LLC */
        return ipx_parse(packet + headerLen, pktSize - headerLen, jparent);
    }

    RETURN_DATA_AFTER(headerLen)
}
