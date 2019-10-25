/* ipx_parser.c
 *
 * This file contains functions to parse IPX (Novell) headers.
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
struct __attribute__ ((__packed__)) ipx_header_s {
	uint16_t chksum;
	uint16_t pktLen;
	uint8_t transCtrl;
	uint8_t type;
	uint32_t dstNet;
	uint8_t dstNode[6];
	uint16_t dstSocket;
	uint32_t srcNet;
	uint8_t srcNode[6];
	uint16_t srcSocket;
};
#pragma GCC diagnostic pop

typedef struct ipx_header_s ipx_header_t;

/*
 *  This function parses the bytes in the received packet to extract IPX metadata.
 *
 *  its parameters are:
 *    - a pointer on the list of bytes representing the packet
 *        the first byte must be the beginning of the IPX header
 *    - the size of the list passed as first parameter
 *    - a pointer on a json_object, containing all the metadata recovered so far
 *      this is also where IPX metadata will be added
 *
 *  This function returns a structure containing the data unprocessed by this parser
 *  or the ones after (as a list of bytes), and the length of this data.
*/
data_ret_t *ipx_parse(const uchar *packet, int pktSize, struct json_object *jparent) {

	DBGPRINTF("entered ipx_parse\n");
	DBGPRINTF("packet size %d\n", pktSize);

	if (pktSize < 30) {  /* too short for IPX header */
		DBGPRINTF("IPX packet too small : %d\n", pktSize);
		RETURN_DATA_AFTER(0)
	}

	char ipxSrcNode[20], ipxDstNode[20];
	ipx_header_t *ipx_header = (ipx_header_t *)packet;

	snprintf(ipxDstNode, sizeof(ipxDstNode), "%02x:%02x:%02x:%02x:%02x:%02x", ipx_header->dstNode[0],
			 ipx_header->dstNode[1], ipx_header->dstNode[2], ipx_header->dstNode[3], ipx_header->dstNode[4],
			 ipx_header->dstNode[5]);

	snprintf(ipxSrcNode, sizeof(ipxSrcNode), "%02x:%02x:%02x:%02x:%02x:%02x", ipx_header->srcNode[0],
			 ipx_header->srcNode[1], ipx_header->srcNode[2], ipx_header->srcNode[3], ipx_header->srcNode[4],
			 ipx_header->srcNode[5]);

	json_object_object_add(jparent, "IPX_transCtrl", json_object_new_int(ipx_header->transCtrl));
	json_object_object_add(jparent, "IPX_type", json_object_new_int(ipx_header->type));
	json_object_object_add(jparent, "IPX_dest_net", json_object_new_int(ntohl(ipx_header->dstNet)));
	json_object_object_add(jparent, "IPX_src_net", json_object_new_int(ntohl(ipx_header->srcNet)));
	json_object_object_add(jparent, "IPX_dest_node", json_object_new_string(ipxDstNode));
	json_object_object_add(jparent, "IPX_src_node", json_object_new_string(ipxSrcNode));
	json_object_object_add(jparent, "IPX_dest_socket", json_object_new_int(ntohs(ipx_header->dstSocket)));
	json_object_object_add(jparent, "IPX_src_socket", json_object_new_int(ntohs(ipx_header->srcSocket)));

	RETURN_DATA_AFTER(30)
}
