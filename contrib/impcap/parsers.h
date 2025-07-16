/* parser.h
 *
 * This file contains the prototypes of all the parsers available within impcap.
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
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <stdarg.h>
#include <ctype.h>
#include <pcap.h>

#include "rsyslog.h"
#include "msg.h"
#include "dirty.h"

#ifdef __FreeBSD__
    #include <sys/socket.h>
#else

    #include <netinet/ether.h>

#endif

#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip6.h>
#include <netinet/ip_icmp.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <net/ethernet.h>
#include <arpa/inet.h>

#ifndef INCLUDED_PARSER_H
    #define INCLUDED_PARSER_H 1

/* data return structure */
struct data_ret_s {
    size_t size;
    char *pData;
};
typedef struct data_ret_s data_ret_t;

    #define RETURN_DATA_AFTER(x)                          \
        data_ret_t *retData = malloc(sizeof(data_ret_t)); \
        if (pktSize > x) {                                \
            retData->size = pktSize - x;                  \
            retData->pData = (char *)packet + x;          \
        } else {                                          \
            retData->size = 0;                            \
            retData->pData = NULL;                        \
        }                                                 \
        return retData;

/* --- handlers prototypes --- */
void packet_parse(uchar *arg, const struct pcap_pkthdr *pkthdr, const uchar *packet);

data_ret_t *eth_parse(const uchar *packet, int pktSize, struct json_object *jparent);

data_ret_t *llc_parse(const uchar *packet, int pktSize, struct json_object *jparent);

data_ret_t *ipx_parse(const uchar *packet, int pktSize, struct json_object *jparent);

data_ret_t *ipv4_parse(const uchar *packet, int pktSize, struct json_object *jparent);

data_ret_t *icmp_parse(const uchar *packet, int pktSize, struct json_object *jparent);

data_ret_t *tcp_parse(const uchar *packet, int pktSize, struct json_object *jparent);

data_ret_t *udp_parse(const uchar *packet, int pktSize, struct json_object *jparent);

data_ret_t *ipv6_parse(const uchar *packet, int pktSize, struct json_object *jparent);

data_ret_t *arp_parse(const uchar *packet, int pktSize, struct json_object *jparent);

data_ret_t *rarp_parse(const uchar *packet, int pktSize, struct json_object *jparent);

data_ret_t *ah_parse(const uchar *packet, int pktSize, struct json_object *jparent);

data_ret_t *esp_parse(const uchar *packet, int pktSize, struct json_object *jparent);

data_ret_t *smb_parse(const uchar *packet, int pktSize, struct json_object *jparent);

data_ret_t *ftp_parse(const uchar *packet, int pktSize, struct json_object *jparent);

data_ret_t *http_parse(const uchar *packet, int pktSize, struct json_object *jparent);

data_ret_t *dns_parse(const uchar *packet, int pktSize, struct json_object *jparent);


// inline function definitions
static inline data_ret_t *dont_parse(const uchar *packet,
                                     int pktSize,
                                     __attribute__((unused)) struct json_object *jparent);

static inline data_ret_t *eth_proto_parse(uint16_t ethProto,
                                          const uchar *packet,
                                          int pktSize,
                                          struct json_object *jparent);

static inline data_ret_t *ip_proto_parse(uint16_t ipProto,
                                         const uchar *packet,
                                         int pktSize,
                                         struct json_object *jparent);

/*
 *  Mock function to do no parsing when protocol is not a valid number
 */
static inline data_ret_t *dont_parse(const uchar *packet,
                                     int pktSize,
                                     __attribute__((unused)) struct json_object *jparent) {
    DBGPRINTF("protocol not handled\n");
    RETURN_DATA_AFTER(0)
}

// proto code handlers
static inline data_ret_t *eth_proto_parse(uint16_t ethProto,
                                          const uchar *packet,
                                          int pktSize,
                                          struct json_object *jparent) {
    switch (ethProto) {
        case ETHERTYPE_IP:
            return ipv4_parse(packet, pktSize, jparent);
        case ETHERTYPE_IPV6:
            return ipv6_parse(packet, pktSize, jparent);
        case ETHERTYPE_ARP:
            return arp_parse(packet, pktSize, jparent);
        case ETHERTYPE_REVARP:
            return rarp_parse(packet, pktSize, jparent);
        case ETHERTYPE_IPX:
            return ipx_parse(packet, pktSize, jparent);
        default:
            return dont_parse(packet, pktSize, jparent);
    }
}

static inline data_ret_t *ip_proto_parse(uint16_t ipProto,
                                         const uchar *packet,
                                         int pktSize,
                                         struct json_object *jparent) {
    switch (ipProto) {
        case IPPROTO_TCP:
            return tcp_parse(packet, pktSize, jparent);
        case IPPROTO_UDP:
            return udp_parse(packet, pktSize, jparent);
        case IPPROTO_ICMP:
            return icmp_parse(packet, pktSize, jparent);
        default:
            return dont_parse(packet, pktSize, jparent);
    }
}

#endif /* INCLUDED_PARSER_H */
