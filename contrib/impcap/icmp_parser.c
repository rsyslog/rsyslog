/* icmp_parser.c
 *
 * This file contains functions to parse ICMP headers.
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

struct icmp_header_s {
	uint8_t type;
	uint8_t code;
	uint16_t checksum;
	uint8_t data[];
};

typedef struct icmp_header_s icmp_header_t;

/*
 *  This function parses the bytes in the received packet to extract ICMP metadata.
 *
 *  its parameters are:
 *    - a pointer on the list of bytes representing the packet
 *        the first byte must be the beginning of the ICMP header
 *    - the size of the list passed as first parameter
 *    - a pointer on a json_object, containing all the metadata recovered so far
 *      this is also where ICMP metadata will be added
 *
 *  This function returns a structure containing the data unprocessed by this parser
 *  or the ones after (as a list of bytes), and the length of this data.
*/
data_ret_t *icmp_parse(const uchar *packet, int pktSize, struct json_object *jparent) {
	DBGPRINTF("icmp_parse\n");
	DBGPRINTF("packet size %d\n", pktSize);

	if (pktSize < 8) {
		DBGPRINTF("ICMP packet too small : %d\n", pktSize);
		RETURN_DATA_AFTER(0);
	}

	/* Union to prevent cast from uchar to icmp_header_t */
	union {
		const uchar *pck;
		icmp_header_t *hdr;
	} icmp_header_to_char;

	icmp_header_to_char.pck = packet;
	icmp_header_t *icmp_header = icmp_header_to_char.hdr;

	json_object_object_add(jparent, "net_icmp_type", json_object_new_int(icmp_header->type));
	json_object_object_add(jparent, "net_icmp_code", json_object_new_int(icmp_header->code));
	json_object_object_add(jparent, "icmp_checksum", json_object_new_int(ntohs(icmp_header->checksum)));

	RETURN_DATA_AFTER(8)
}
