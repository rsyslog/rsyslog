/* ipv6_parser.c
 *
 * This file contains functions to parse IPv6 headers.
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
typedef struct __attribute__ ((__packed__)) ipv6_header_s {
#ifndef IPV6_VERSION_MASK
#define IPV6_VERSION_MASK   0xF0000000
#endif
#ifndef IPV6_TC_MASK
#define IPV6_TC_MASK        0x0FF00000
#endif
#ifndef IPV6_FLOW_MASK
#define IPV6_FLOW_MASK        0x000FFFFF
#endif
	uint32_t vtf;
	uint16_t dataLength;
	uint8_t nextHeader;
#define IPV6_NHDR_HBH     0
#define IPV6_NHDR_TCP     6
#define IPV6_NHDR_UDP     17
#define IPV6_NHDR_ENCIP6  41
#define IPV6_NHDR_ROUT    43
#define IPV6_NHDR_FRAG    44
#define IPV6_NHDR_RRSV    46
#define IPV6_NHDR_SEC     50
#define IPV6_NHDR_AUTH    51
#define IPV6_NHDR_ICMP6   58
#define IPV6_NHDR_NONHDR  59
#define IPV6_NHDR_DOPTS   60

	uint8_t hopLimit;
	uint8_t addrSrc[16];
	uint8_t addrDst[16];
} ipv6_header_t;
#pragma GCC diagnostic pop

#ifndef IPV6_VERSION
#define IPV6_VERSION(h) (ntohl(h->vtf) & IPV6_VERSION_MASK)>>28
#endif
#ifndef IPV6_TC
#define IPV6_TC(h)            (ntohl(h->vtf) & IPV6_TC_MASK)>>20
#endif
#ifndef IPV6_FLOW
#define IPV6_FLOW(h)        (ntohl(h->vtf) & IPV6_FLOW_MASK)
#endif

/* extension headers */
typedef struct hbh_header_s {
	uint8_t nextHeader;
	uint8_t hLength;
	uint8_t *pOptions;
} hbh_header_t;

typedef struct dest_header_s {
	uint8_t nextHeader;
	uint8_t hLength;
	uint8_t *pOptions;
} dest_header_t;

typedef struct route_header_s {
	uint8_t nextHeader;
	uint8_t hLength;
	uint8_t rType;
	uint8_t segsLeft;
	uint32_t reserved;
	uint8_t addrs[16];
} route_header_t;

typedef struct frag_header_s {
	uint8_t nextHeader;
	uint8_t reserved;
	uint16_t offsetFlags;
	uint32_t id;
} frag_header_t;

static inline uint8_t hbh_header_parse(const uchar **packet, int *pktSize) {
	DBGPRINTF("hbh_header_parse\n");

	/* Union to prevent cast from uchar to hbh_header_t */
	union {
		const uchar *pck;
		hbh_header_t *hdr;
	} hbh_header_to_char;

	hbh_header_to_char.pck = *packet;
	hbh_header_t *hbh_header = hbh_header_to_char.hdr;

	/* hbh_header->hLength is the number of octets of header in 8-octet units minus 1
	* the header length SHOULD be a multiple of 8 */
	uint8_t hByteLength = hbh_header->hLength * 8 + 8;
	DBGPRINTF("hByteLength: %d\n", hByteLength);
	*pktSize -= hByteLength;
	*packet += hByteLength;

	return hbh_header->nextHeader;
}

static inline uint8_t dest_header_parse(const uchar **packet, int *pktSize) {
	DBGPRINTF("dest_header_parse\n");

	/* Union to prevent cast from uchar to dest_header_t */
	union {
		const uchar *pck;
		dest_header_t *hdr;
	} dest_header_to_char;

	dest_header_to_char.pck = *packet;
	dest_header_t *dest_header = dest_header_to_char.hdr;

	/* dest_header->hLength is the number of octets of header in 8-octet units minus 1
	 * the header length SHOULD be a multiple of 8 */
	uint8_t hByteLength = dest_header->hLength * 8 + 8;
	DBGPRINTF("hByteLength: %d\n", hByteLength);
	*pktSize -= hByteLength;
	*packet += hByteLength;

	return dest_header->nextHeader;
}

static inline uint8_t route_header_parse(const uchar **packet, int *pktSize, struct json_object *jparent) {
	DBGPRINTF("route_header_parse\n");

	/* Union to prevent cast from uchar to route_header_t */
	union {
		const uchar *pck;
		route_header_t *hdr;
	} route_header_to_char;

	route_header_to_char.pck = *packet;
	route_header_t *route_header = route_header_to_char.hdr;

	/* route_header->hLength is the number of octets of header in 8-octet units minus 1
	* the header length (in bytes) SHOULD be a multiple of 8 */
	uint8_t hByteLength = route_header->hLength * 8 + 8;
	*pktSize -= hByteLength;
	*packet += hByteLength;

	if (route_header->rType == 0) {
		json_object_object_add(jparent, "IP6_route_seg_left", json_object_new_int(route_header->segsLeft));

		hByteLength -= 8;   //leave only length of routing addresses

		char addrStr[40], routeFieldName[20];
		int addrNum = 1;
		uint8_t *addr = &(route_header->addrs[0]);

		//while there is enough space for an IPv6 address
		while (hByteLength >= 16) {
			inet_ntop(AF_INET6, (void *)addr, addrStr, 40);
			snprintf(routeFieldName, 20, "IP6_route_%d", addrNum++);
			json_object_object_add(jparent, routeFieldName, json_object_new_string((char *)addrStr));

			addr += 16;
			hByteLength -= 16;
		}
	}

	return route_header->nextHeader;
}

#define FRAG_OFFSET_MASK    0xFFF8
#define MFLAG_MASK          0x0001
static inline uint8_t frag_header_parse(const uchar **packet, int *pktSize, struct json_object *jparent) {
	DBGPRINTF("frag_header_parse\n");

	/* Union to prevent cast from uchar to frag_header_t */
	union {
		const uchar *pck;
		frag_header_t *hdr;
	} frag_header_to_char;

	frag_header_to_char.pck = *packet;
	frag_header_t *frag_header = frag_header_to_char.hdr;

	uint16_t flags = ntohs(frag_header->offsetFlags);

	json_object_object_add(jparent, "IP6_frag_offset", json_object_new_int((flags & FRAG_OFFSET_MASK) >> 3));
	json_object_object_add(jparent, "IP6_frag_more", json_object_new_boolean(flags & MFLAG_MASK));
	json_object_object_add(jparent, "IP6_frag_id", json_object_new_int64(frag_header->id));

	*pktSize -= 8;
	*packet += 8;

	return frag_header->nextHeader;
}

/*
 *  This function parses the bytes in the received packet to extract IPv6 metadata.
 *
 *  its parameters are:
 *    - a pointer on the list of bytes representing the packet
 *        the first byte must be the beginning of the IPv6 header
 *    - the size of the list passed as first parameter
 *    - a pointer on a json_object, containing all the metadata recovered so far
 *      this is also where IPv6 metadata will be added
 *
 *  This function returns a structure containing the data unprocessed by this parser
 *  or the ones after (as a list of bytes), and the length of this data.
*/
data_ret_t *ipv6_parse(const uchar *packet, int pktSize, struct json_object *jparent) {
	DBGPRINTF("ipv6_parse\n");
	DBGPRINTF("packet size %d\n", pktSize);

	if (pktSize < 40) { /* too small for IPv6 header + data (header might be longer)*/
		DBGPRINTF("IPv6 packet too small : %d\n", pktSize);
		RETURN_DATA_AFTER(0)
	}

	ipv6_header_t *ipv6_header = (ipv6_header_t *)packet;

	char addrSrc[40], addrDst[40];

	inet_ntop(AF_INET6, (void *)&ipv6_header->addrSrc, addrSrc, 40);
	inet_ntop(AF_INET6, (void *)&ipv6_header->addrDst, addrDst, 40);

	json_object_object_add(jparent, "net_dst_ip", json_object_new_string((char *)addrDst));
	json_object_object_add(jparent, "net_src_ip", json_object_new_string((char *)addrSrc));
	json_object_object_add(jparent, "net_ttl", json_object_new_int(ipv6_header->hopLimit));

	uint8_t nextHeader = ipv6_header->nextHeader;

	packet += sizeof(ipv6_header_t);
	pktSize -= sizeof(ipv6_header_t);

	DBGPRINTF("beginning ext headers scan\n");
	uint8_t hasNext = 1;
	do {
		switch (nextHeader) {
			case IPV6_NHDR_HBH:
				nextHeader = hbh_header_parse(&packet, &pktSize);
				break;
			case IPV6_NHDR_TCP:
				json_object_object_add(jparent, "IP_proto", json_object_new_int(nextHeader));
				return tcp_parse(packet, pktSize, jparent);
			case IPV6_NHDR_UDP:
				json_object_object_add(jparent, "IP_proto", json_object_new_int(nextHeader));
				return udp_parse(packet, pktSize, jparent);
			case IPV6_NHDR_ENCIP6:
				hasNext = 0;
				break;
			case IPV6_NHDR_ROUT:
				nextHeader = route_header_parse(&packet, &pktSize, jparent);
				break;
			case IPV6_NHDR_FRAG:
				nextHeader = frag_header_parse(&packet, &pktSize, jparent);
				break;
			case IPV6_NHDR_RRSV:
				hasNext = 0;
				break;
			case IPV6_NHDR_SEC:
				hasNext = 0;
				break;
			case IPV6_NHDR_AUTH:
				hasNext = 0;
				break;
			case IPV6_NHDR_ICMP6:
				json_object_object_add(jparent, "IP_proto", json_object_new_int(nextHeader));
				return icmp_parse(packet, pktSize, jparent);
			case IPV6_NHDR_NONHDR:
				hasNext = 0;
				break;
			case IPV6_NHDR_DOPTS:
				nextHeader = dest_header_parse(&packet, &pktSize);
				break;
			default:
				hasNext = 0;
				break;
		}
	} while (hasNext);

	json_object_object_add(jparent, "IP_proto", json_object_new_int(nextHeader));
	RETURN_DATA_AFTER(0)
}
