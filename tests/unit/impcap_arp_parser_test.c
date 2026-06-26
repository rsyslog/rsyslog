/* Regression test for contrib/impcap ARP/RARP address-length handling.
 *
 * impcap is optional and only exposes this path when configured to process
 * captured L2 traffic. Malformed ARP/RARP frames can still be attacker-provided
 * on such interfaces, so the parser must not trust packet-controlled
 * hwAddrLen/pAddrLen fields to form offsets beyond the captured packet.
 *
 * The malformed cases below use minimal captured ARP/RARP payloads with either
 * oversized address lengths or Ethernet/IPv4 lengths that are too short for the
 * fixed-width address converters. Before the fix, guard-page mode made the real
 * parser crash while converting the destination Ethernet address. The oracle
 * here is that malformed packets return without address metadata, while a
 * normal Ethernet/IPv4 ARP and RARP packet still produces the expected parsed
 * fields. The demonstrated impact is crash/DoS; this test does not claim code
 * execution or information disclosure.
 */

#define _GNU_SOURCE
#include "config.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "contrib/impcap/parsers.h"

#define CHECK(cond)                                                                  \
    do {                                                                             \
        if (!(cond)) {                                                               \
            fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #cond); \
            exit(1);                                                                 \
        }                                                                            \
    } while (0)

int Debug = 0;
void r_dbgprintf(const char *srcname, const char *fmt, ...) {
    (void)srcname;
    (void)fmt;
}

static void set_u16(unsigned char *packet, size_t off, unsigned value) {
    packet[off] = (unsigned char)(value >> 8);
    packet[off + 1] = (unsigned char)value;
}

static void fill_arp_header(unsigned char *packet, unsigned opCode) {
    memset(packet, 0, 28);
    set_u16(packet, 0, 1); /* hardware type: Ethernet */
    set_u16(packet, 2, ETHERTYPE_IP); /* protocol type: IPv4 */
    packet[4] = 6; /* Ethernet address length */
    packet[5] = 4; /* IPv4 address length */
    set_u16(packet, 6, opCode);
}

static void fill_valid_arp(unsigned char *packet, unsigned opCode) {
    fill_arp_header(packet, opCode);

    const unsigned char hwSrc[6] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55};
    const unsigned char pSrc[4] = {192, 0, 2, 1};
    const unsigned char hwDst[6] = {0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff};
    const unsigned char pDst[4] = {198, 51, 100, 2};

    memcpy(packet + 8, hwSrc, sizeof(hwSrc));
    memcpy(packet + 14, pSrc, sizeof(pSrc));
    memcpy(packet + 18, hwDst, sizeof(hwDst));
    memcpy(packet + 24, pDst, sizeof(pDst));
}

static void fill_malformed_arp(unsigned char *packet) {
    fill_arp_header(packet, 1);
    packet[4] = 250; /* attacker-controlled length exceeds captured packet */
}

static long guarded_page_size(void) {
    const long page = sysconf(_SC_PAGESIZE);
    CHECK(page > 28);
    return page;
}

static unsigned char *make_guarded_malformed_packet(void) {
    const long page = guarded_page_size();

    unsigned char *map = mmap(NULL, (size_t)page * 2, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    CHECK(map != MAP_FAILED);
    CHECK(mprotect(map + page, (size_t)page, PROT_NONE) == 0);

    unsigned char *packet = map + page - 28;
    fill_malformed_arp(packet);
    return packet;
}

static void free_guarded_packet(unsigned char *packet) {
    const long page = guarded_page_size();
    unsigned char *map = packet + 28 - page;
    CHECK(munmap(map, (size_t)page * 2) == 0);
}

static int object_has_key(struct json_object *obj, const char *key) {
    struct json_object *tmp = NULL;
    return json_object_object_get_ex(obj, key, &tmp);
}

static void check_json_string(struct json_object *obj, const char *key, const char *expected) {
    struct json_object *value = NULL;
    CHECK(json_object_object_get_ex(obj, key, &value));
    const char *actual = json_object_get_string(value);
    if (strcmp(actual, expected) != 0) {
        fprintf(stderr, "%s: expected '%s', got '%s'\n", key, expected, actual);
        exit(1);
    }
}

static void check_no_address_fields(struct json_object *obj, const char *prefix) {
    char key[32];

    snprintf(key, sizeof(key), "%s_hwSrc", prefix);
    CHECK(!object_has_key(obj, key));
    snprintf(key, sizeof(key), "%s_hwDst", prefix);
    CHECK(!object_has_key(obj, key));
    snprintf(key, sizeof(key), "%s_pSrc", prefix);
    CHECK(!object_has_key(obj, key));
    snprintf(key, sizeof(key), "%s_pDst", prefix);
    CHECK(!object_has_key(obj, key));
}

typedef data_ret_t *(*arp_test_parser_t)(const uchar *, int, struct json_object *);

static void run_short_fixed_width_length_case(
    arp_test_parser_t parse, const char *prefix, unsigned opCode, unsigned hwAddrLen, unsigned pAddrLen) {
    unsigned char packet[28];
    struct json_object *j = json_object_new_object();
    CHECK(j != NULL);

    fill_valid_arp(packet, opCode);
    packet[4] = (unsigned char)hwAddrLen;
    packet[5] = (unsigned char)pAddrLen;

    data_ret_t *ret = parse(packet, sizeof(packet), j);
    CHECK(ret != NULL);
    CHECK(ret->size == 0);
    free(ret);

    check_no_address_fields(j, prefix);
    json_object_put(j);
}

static void test_malformed_lengths_do_not_parse_addresses(void) {
    unsigned char *packet = make_guarded_malformed_packet();
    struct json_object *j = json_object_new_object();
    CHECK(j != NULL);

    data_ret_t *ret = arp_parse(packet, 28, j);
    CHECK(ret != NULL);
    CHECK(ret->size == 0);
    free(ret);

    CHECK(object_has_key(j, "ARP_hwType"));
    CHECK(object_has_key(j, "ARP_pType"));
    CHECK(object_has_key(j, "ARP_op"));
    CHECK(!object_has_key(j, "ARP_hwSrc"));
    CHECK(!object_has_key(j, "ARP_hwDst"));
    CHECK(!object_has_key(j, "ARP_pSrc"));
    CHECK(!object_has_key(j, "ARP_pDst"));

    json_object_put(j);
    free_guarded_packet(packet);
}

static void test_malformed_rarp_lengths_do_not_parse_addresses(void) {
    unsigned char *packet = make_guarded_malformed_packet();
    struct json_object *j = json_object_new_object();
    CHECK(j != NULL);

    data_ret_t *ret = rarp_parse(packet, 28, j);
    CHECK(ret != NULL);
    CHECK(ret->size == 0);
    free(ret);

    CHECK(object_has_key(j, "RARP_hwType"));
    CHECK(object_has_key(j, "RARP_pType"));
    CHECK(object_has_key(j, "RARP_op"));
    CHECK(!object_has_key(j, "RARP_hwSrc"));
    CHECK(!object_has_key(j, "RARP_hwDst"));
    CHECK(!object_has_key(j, "RARP_pSrc"));
    CHECK(!object_has_key(j, "RARP_pDst"));

    json_object_put(j);
    free_guarded_packet(packet);
}

static void test_short_fixed_width_lengths_do_not_parse_addresses(void) {
    run_short_fixed_width_length_case(arp_parse, "ARP", 1, 5, 4);
    run_short_fixed_width_length_case(arp_parse, "ARP", 1, 6, 3);
    run_short_fixed_width_length_case(rarp_parse, "RARP", 3, 5, 4);
    run_short_fixed_width_length_case(rarp_parse, "RARP", 3, 6, 3);
}

static void test_valid_arp_still_parses_addresses(void) {
    unsigned char packet[28];
    struct json_object *j = json_object_new_object();
    CHECK(j != NULL);
    fill_valid_arp(packet, 1);

    data_ret_t *ret = arp_parse(packet, sizeof(packet), j);
    CHECK(ret != NULL);
    CHECK(ret->size == 0);
    free(ret);

    check_json_string(j, "ARP_hwSrc", "0:11:22:33:44:55");
    check_json_string(j, "ARP_hwDst", "aa:bb:cc:dd:ee:ff");
    check_json_string(j, "ARP_pSrc", "192.0.2.1");
    check_json_string(j, "ARP_pDst", "198.51.100.2");

    json_object_put(j);
}

static void test_valid_rarp_still_parses_addresses(void) {
    unsigned char packet[28];
    struct json_object *j = json_object_new_object();
    CHECK(j != NULL);
    fill_valid_arp(packet, 3);

    data_ret_t *ret = rarp_parse(packet, sizeof(packet), j);
    CHECK(ret != NULL);
    CHECK(ret->size == 0);
    free(ret);

    check_json_string(j, "RARP_hwSrc", "0:11:22:33:44:55");
    check_json_string(j, "RARP_hwDst", "aa:bb:cc:dd:ee:ff");
    check_json_string(j, "RARP_pSrc", "192.0.2.1");
    check_json_string(j, "RARP_pDst", "198.51.100.2");

    json_object_put(j);
}

#include "../../contrib/impcap/arp_parser.c"

int main(void) {
    test_malformed_lengths_do_not_parse_addresses();
    test_malformed_rarp_lengths_do_not_parse_addresses();
    test_short_fixed_width_lengths_do_not_parse_addresses();
    test_valid_arp_still_parses_addresses();
    test_valid_rarp_still_parses_addresses();
    return 0;
}
