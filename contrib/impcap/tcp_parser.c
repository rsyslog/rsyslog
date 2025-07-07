/* tcp_parser.c
 *
 * This file contains functions to parse TCP headers.
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

#define SMB_PORT 445
#define HTTP_PORT 80
#define HTTP_PORT_ALT 8080
#define FTP_PORT 21
#define FTP_PORT_DATA 20

struct tcp_header_s {
	uint16_t srcPort;
	uint16_t dstPort;
	uint32_t seq;
	uint32_t ack;
	uint8_t dor;
	uint8_t flags;
	uint16_t windowSize;
	uint16_t checksum;
	uint16_t urgPointer;
	uint8_t options[];
};

typedef struct tcp_header_s tcp_header_t;

static char flagCodes[10] = "FSRPAUECN";

/*
 *  This function parses the bytes in the received packet to extract TCP metadata.
 *
 *  its parameters are:
 *    - a pointer on the list of bytes representing the packet
 *        the first byte must be the beginning of the TCP header
 *    - the size of the list passed as first parameter
 *    - a pointer on a json_object, containing all the metadata recovered so far
 *      this is also where TCP metadata will be added
 *
 *  This function returns a structure containing the data unprocessed by this parser
 *  or the ones after (as a list of bytes), and the length of this data.
*/
data_ret_t *tcp_parse(const uchar *packet, int pktSize, struct json_object *jparent) {
	DBGPRINTF("tcp_parse\n");
	DBGPRINTF("packet size %d\n", pktSize);

	if (pktSize < 20) {
		DBGPRINTF("TCP packet too small : %d\n", pktSize);
		RETURN_DATA_AFTER(0)
	}

	/* Union to prevent cast from uchar to tcp_header_t */
	union {
		const uchar *pck;
		tcp_header_t *hdr;
	} tcp_header_to_char;

	tcp_header_to_char.pck = packet;
	tcp_header_t *tcp_header = tcp_header_to_char.hdr;

	uint8_t i, pos = 0;
	char flags[10] = {0};

	for (i = 0; i < 8; ++i) {
		if (tcp_header->flags & (0x01 << i))
			flags[pos++] = flagCodes[i];
	}
	if (tcp_header->dor & 0x01)
		flags[pos++] = flagCodes[9];

	uint16_t srcPort = ntohs(tcp_header->srcPort);
	uint16_t dstPort = ntohs(tcp_header->dstPort);

	uint8_t headerLength = (tcp_header->dor & 0xF0) >> 2; //>>4 to offset but <<2 to get offset as bytes

	json_object_object_add(jparent, "net_src_port", json_object_new_int(srcPort));
	json_object_object_add(jparent, "net_dst_port", json_object_new_int(dstPort));
	json_object_object_add(jparent, "TCP_seq_number", json_object_new_int64(ntohl(tcp_header->seq)));
	json_object_object_add(jparent, "TCP_ack_number", json_object_new_int64(ntohl(tcp_header->ack)));
	json_object_object_add(jparent, "net_flags", json_object_new_string(flags));

	if (srcPort == SMB_PORT || dstPort == SMB_PORT) {
		return smb_parse(packet + headerLength, pktSize - headerLength, jparent);
	}
	if (srcPort == FTP_PORT || dstPort == FTP_PORT || srcPort == FTP_PORT_DATA || dstPort == FTP_PORT_DATA) {
		return ftp_parse(packet + headerLength, pktSize - headerLength, jparent);
	}
	if (srcPort == HTTP_PORT || dstPort == HTTP_PORT ||
		srcPort == HTTP_PORT_ALT || dstPort == HTTP_PORT_ALT) {
		return http_parse(packet + headerLength, pktSize - headerLength, jparent);
	}
	DBGPRINTF("tcp return after header length (%u)\n", headerLength);
	RETURN_DATA_AFTER(headerLength)
}
