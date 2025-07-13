/* dns_parser.c
 *
 * This file contains functions to parse DNS headers.
 *
 * File begun on 2018-11-13
 *
 * Created by:
 *  - Kevin Guillemot (kevin.guillemot@advens.fr)
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


/* List of RCodes defined in RFC6895 : https://tools.ietf.org/html/rfc6895 */
static const char *dns_rcodes[] = {
        "NoError",  // 0
        "FormErr",  // 1
        "ServFail", // 2
        "NXDomain", // 3
        "NotImp",   // 4
        "Refused",  // 5
        "YXDomain", // 6
        "YXRRSet",  // 7
        "NXRRSet",  // 8
        "NotAuth",  // 9
        "NotZone",  // 10
        "",         // 11 - Reserved
        "",         // 12 - Reserved
        "",         // 13 - Reserved
        "",         // 14 - Reserved
        "",         // 15 - Reserved
        "BADVERS|BADSIG", // 16
        "BADKEY",   // 17
        "BADTIME",  // 18
        "BADMODE",  // 19
        "BADNAME",  // 20
        "BADALG",   // 21
        "BADTRUNC",  // 22
        /* Reserved for private use */
        NULL
};

/* List of record types (maybe not complete) */
static const char *dns_types[] = {
        0,
        "A",        // 1
        "NS",       // 2
        "MD",       // 3
        "MF",       // 4
        "CNAME",    // 5
        "SOA",      // 6
        "MB",       // 7
        "MG",       // 8
        "MR",       // 9
        "NULL",     // 10
        "WKS",      // 11
        "PTR",      // 12
        "HINFO",    // 13
        "MINFO",    // 14
        "MX",       // 15
        "TXT",      // 16
        "RP",       // 17
        "AFSDB",    // 18
        "X25",      // 19
        "ISDN",     // 20
        "RT",       // 21
        "NSAP",     // 22
        "NSAP-PTR", // 23
        "SIG",      // 24
        "KEY",      // 25
        "PX",       // 26
        "GPOS",     // 27
        "AAAA",     // 28
        "LOC",      // 29
        "NXT",      // 30
        "EID",      // 31
        "NIMLOC",   // 32
        "SRV",      // 33
        "ATMA",     // 34
        "NAPTR",    // 35
        "KX",       // 36
        "CERT",     // 37
        "A6",       // 38
        "DNAME",    // 39
        "SINK",     // 40
        "OPT",      // 41
        "APL",      // 42
        "DS",       // 43
        "SSHFP",    // 44
        "IPSECKEY", // 45
        "RRSIG",    // 46
        "NSEC",     // 47
        "DNSKEY",   // 48
        "DHCID",    // 49
        "NSEC3",    // 50
        "NSEC3PARAM",   // 51
        "TLSA",     // 51
        "SMIMEA",   // 52
        "Unassigned",   // 53
        "HIP",      // 53
        "NINFO",    // 54
        "RKEY",     // 55
        "TALINK",   // 56
        "CDS",      // 57
        "CDNSKEY",  // 58
        "OPENPGPKEY",   // 59
        "CSYNC",    // 60
        "ZONEMD",   // 61
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        "SPF",      // 99
        "UINFO",    // 100
        "UID",      // 101
        "GID",      // 102
        "UNSPEC",   // 103
        "NID",      // 104
        "L32",      // 105
        "L64",      // 106
        "LP",       // 107
        "EUI48",    // 108
        "EUI64",    // 109
        /* Reserved for private use */
        NULL
};
/* Part 2, since 249. To prevent useless large buffer in memory */
static const char *dns_types2[] = {
        "TKEY",
        "TSIG",
        "IXFR",
        "AXFR",
        "MAILB",
        "MAILA",
        "*",
        "URI",
        "CAA",
        "AVC",
        "DOA",
        "AMTRELAY",
        NULL
};
/* Part 3, since 32768. To prevent useless large buffer in memory */
static const char *dns_types3[] = {
        "TA",
        "DLV",
        NULL
};


/* This function takes an integer as parameter
 *  and returns the corresponding string type of DNS query
 */
static const char *get_type(uint16_t x) {
    const char **types = NULL;
    uint16_t len_types3 = (sizeof(dns_types3) / sizeof(char *)) - 1;
    uint16_t len_types2 = (sizeof(dns_types2) / sizeof(char *)) - 1;
    uint16_t len_types = (sizeof(dns_types) / sizeof(char *)) - 1;
    if (x >= 32768 && x < 32768 + len_types3) {
        types = dns_types3;
        x -= 32768;
    }
    else if (x >= 249 && x < 249 + len_types2) {
        types = dns_types2;
        x -= 249;
    }
    else if (x > 0 && x < len_types)
        types = dns_types;
    else
        return "UNKNOWN";
    if (types[x] != NULL)
        return types[x];
    return "UNKNOWN";
}


/* This function takes an integer as parameter
 *  and returns the corresponding string class of DNS query
 */
static const char *get_class(uint16_t x) {
    switch (x) {
        case 1:
            return "IN";
        case 3:
            return "CH";
        case 4:
            return "HS";
        case 254:
            return "QCLASS NONE";
        case 255:
            return "QCLASS *";
        default:
            // No action needed for other cases
            break;
    }
    return "UNKNOWN";
}


/*
 *  This function parses the bytes in the received packet to extract DNS metadata.
 *
 *  its parameters are:
 *    - a pointer on the list of bytes representing the packet
 *    - the size of the list passed as first parameter
 *    - a pointer on a json_object, containing all the metadata recovered so far
 *      this is also where DNS metadata will be added
 *
 *  This function returns a structure containing the data unprocessed by this parser
 *  or the ones after (as a list of bytes), and the length of this data.
*/
data_ret_t *dns_parse(const uchar *packet, int pktSize, struct json_object *jparent) {
    const uchar *packet_ptr = packet;
    const uchar *end_packet = packet + pktSize;
    DBGPRINTF("dns_parse\n");
    DBGPRINTF("packet size %d\n", pktSize);

    /* Union to prevent cast from uchar to smb_header_t */
    union {
        unsigned short int *two_bytes;
        const uchar *pckt;
    } union_short_int;

    /* Get transaction id */
    union_short_int.pckt = packet_ptr;
    unsigned short int transaction_id = ntohs(*(union_short_int.two_bytes));
    //DBGPRINTF("transaction_id = %02x \n", transaction_id);
    union_short_int.pckt += 2;

    /* Get flags */
    unsigned short int flags = ntohs(*(union_short_int.two_bytes));
    //DBGPRINTF("flags = %02x \n", flags);

    /* Get response flag */
    unsigned short int response_flag = (flags >> 15) & 0b1; // Get the left bit
    //DBGPRINTF("response_flag = %02x \n", response_flag);

    /* Get Opcode */
    unsigned short int opcode = (flags >> 11) & 0b1111;
    //DBGPRINTF("opcode = %02x \n", opcode);

    /* Verify Z: reserved bit */
    unsigned short int reserved = (flags >> 6) & 0b1;
    //DBGPRINTF("reserved = %02x \n", reserved);
    /* Reserved bit MUST be 0 */
    if (reserved != 0) {
        DBGPRINTF("DNS packet reserved bit (Z) is not 0, aborting message. \n");
        RETURN_DATA_AFTER(0)
    }

    /* Get reply code : 4 last bits */
    unsigned short int reply_code = flags & 0b1111;
    //DBGPRINTF("reply_code = %02x \n", reply_code);

    union_short_int.pckt += 2;

    /* Get QDCOUNT */
    unsigned short int query_count = ntohs(*(union_short_int.two_bytes));
    //DBGPRINTF("query_count = %02x \n", query_count);
    union_short_int.pckt += 2;

    /* Get ANCOUNT */
    unsigned short int answer_count = ntohs(*(union_short_int.two_bytes));
    //DBGPRINTF("answer_count = %02x \n", answer_count);
    union_short_int.pckt += 2;

    /* Get NSCOUNT */
    unsigned short int authority_count = ntohs(*(union_short_int.two_bytes));
    //DBGPRINTF("authority_count = %02x \n", authority_count);
    union_short_int.pckt += 2;

    /* Get ARCOUNT */
    unsigned short int additionnal_count = ntohs(*(union_short_int.two_bytes));
    //DBGPRINTF("additionnal_count = %02x \n", additionnal_count);
    union_short_int.pckt += 2;
    packet_ptr = union_short_int.pckt;

    fjson_object *queries = NULL;
    if ((queries = json_object_new_array()) == NULL) {
        DBGPRINTF("impcap::dns_parser: Cannot create new json array. Stopping.\n");
        RETURN_DATA_AFTER(0)
    }

    // For each query of query_count
    int query_cpt = 0;
    while (query_cpt < query_count && packet_ptr < end_packet) {
        size_t query_size = strnlen((const char *)packet_ptr, (size_t)(end_packet - packet_ptr));
        // Check if query is valid (max 255 bytes, plus a '\0')
        if (query_size >= 256) {
            DBGPRINTF("impcap::dns_parser: Length of domain queried is > 255. Stopping.\n");
            break;
        }
        // Check if remaining data is enough to hold query + '\0' + 4 bytes (QTYPE and QCLASS fields)
        if (query_size + 5 > (size_t)(end_packet - packet_ptr)) {
            DBGPRINTF("impcap::dns_parser: packet size too small to parse query. Stopping.\n");
            break;
        }
        fjson_object *query = NULL;
        if ((query = json_object_new_object()) == NULL) {
            DBGPRINTF("impcap::dns_parser: Cannot create new json object. Stopping.\n");
            break;
        }
        char domain_query[256] = {0};
        uchar nb_char = *packet_ptr;
        packet_ptr++;
        size_t cpt = 0;
        while (cpt + 1 < query_size) {
            if (nb_char == 0) {
                nb_char = *packet_ptr;
                domain_query[cpt] = '.';
            } else {
                domain_query[cpt] = (char)*packet_ptr;
                nb_char--;
            }
            cpt++;
            packet_ptr++;
        }
        domain_query[cpt] = '\0';
        if (cpt)
            packet_ptr++; // pass the last \0, only if query was not empty
        // DBGPRINTF("Requested domain : '%s' \n", domain_query);

        /* Register the name in dict */
        json_object_object_add(query, "qname", json_object_new_string(domain_query));
        /* Get QTYPE */
        union_short_int.pckt = packet_ptr;
        unsigned short int qtype = ntohs(*(union_short_int.two_bytes));
        //DBGPRINTF("qtype = %02x \n", qtype);
        json_object_object_add(query, "qtype", json_object_new_int((int)qtype));
        json_object_object_add(query, "type", json_object_new_string(get_type(qtype)));
        union_short_int.pckt += 2;
        /* Retrieve QCLASS */
        unsigned short int qclass = ntohs(*(union_short_int.two_bytes));
        //DBGPRINTF("qclass = %02x \n", qclass);
        json_object_object_add(query, "qclass", json_object_new_int((int)qclass));
        json_object_object_add(query, "class", json_object_new_string(get_class(qclass)));
        packet_ptr = union_short_int.pckt + 2;
        /* Register the query in json array */
        json_object_array_add(queries, query);
        query_cpt++;
    }

    json_object_object_add(jparent, "DNS_transaction_id", json_object_new_int((int)transaction_id));

    json_bool is_reponse = FALSE;
    if (response_flag)
        is_reponse = TRUE;
    json_object_object_add(jparent, "DNS_response_flag", json_object_new_boolean(is_reponse));

    json_object_object_add(jparent, "DNS_opcode", json_object_new_int(opcode));
    json_object_object_add(jparent, "DNS_rcode", json_object_new_int((int)reply_code));
    json_object_object_add(jparent, "DNS_error", json_object_new_string(dns_rcodes[reply_code]));
    json_object_object_add(jparent, "DNS_QDCOUNT", json_object_new_int((int)query_count));
    json_object_object_add(jparent, "DNS_ANCOUNT", json_object_new_int((int)answer_count));
    json_object_object_add(jparent, "DNS_NSCOUNT", json_object_new_int((int)authority_count));
    json_object_object_add(jparent, "DNS_ARCOUNT", json_object_new_int((int)additionnal_count));
    json_object_object_add(jparent, "DNS_Names", queries);

    /* Packet has been successfully parsed, there still can be some responses left, but do not process them */
    RETURN_DATA_AFTER(0);
}
