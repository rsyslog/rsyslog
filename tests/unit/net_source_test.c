/*
 * This file is part of rsyslog.
 *
 * Copyright 2026 Rainer Gerhards and Adiscon GmbH.
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
/**
 * @file net_source_test.c
 * @brief Unit coverage for source-address parsing and destination ranking.
 *
 * The oracle for parse tests is the exact accepted/rejected outcome of
 * parse_entry() for each spec, plus (for accepted specs) the decoded address
 * family, textual address, and prefix length. This directly exercises
 * whitespace trimming, optional IPv6 brackets, malformed bracket pairing,
 * multiple '/' separators, and numeric prefix edge cases (range, sign,
 * leading zeros, embedded/leading/trailing garbage, overflow) that are not
 * covered by the end-to-end omfwd shell tests. The oracle for ranking tests
 * is the exact selected bind address order for IPv4 and IPv6, including
 * longest-prefix preference, configuration-order tie-breaking, family
 * isolation, and API-misuse error codes.
 */
#include "config.h"

#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "net_source.h"

#include "../../runtime/net_source.c"

#define CHECK(condition)                                                                    \
    do {                                                                                    \
        if (!(condition)) {                                                                 \
            fprintf(stderr, "CHECK failed at %s:%d: %s\n", __FILE__, __LINE__, #condition); \
            exit(1);                                                                        \
        }                                                                                   \
    } while (0)

/* Parse spec directly and assert it is accepted with the expected decoded
 * address family, textual address, and prefix length.
 */
static void expect_valid(const char *spec, int family, const char *expected_address, uint8_t expected_prefix) {
    net_source_entry_t entry;
    CHECK(parse_entry(&entry, spec, 0) == RS_RET_OK);
    CHECK(entry.address.ss_family == family);
    CHECK(entry.prefix_length == expected_prefix);

    char actual[INET6_ADDRSTRLEN];
    const void *bytes = family == AF_INET ? (const void *)&((struct sockaddr_in *)&entry.address)->sin_addr
                                          : (const void *)&((struct sockaddr_in6 *)&entry.address)->sin6_addr;
    CHECK(inet_ntop(family, bytes, actual, sizeof(actual)) != NULL);
    CHECK(strcmp(actual, expected_address) == 0);
}

/* Parse spec directly and assert it is rejected as malformed. */
static void expect_invalid(const char *spec) {
    net_source_entry_t entry;
    CHECK(parse_entry(&entry, spec, 0) == RS_RET_INVALID_IP);
}

static void check_address(const net_source_entry_t *entry, const char *expected) {
    char actual[INET6_ADDRSTRLEN];
    socklen_t length;
    const struct sockaddr *address = net_source_entry_address(entry, &length);
    struct in_addr address4;
    struct in6_addr address6;
    const void *bytes;
    CHECK(address != NULL);
    if (address->sa_family == AF_INET) {
        memcpy(&address4, (const uint8_t *)address + offsetof(struct sockaddr_in, sin_addr), sizeof(address4));
        bytes = &address4;
    } else {
        memcpy(&address6, (const uint8_t *)address + offsetof(struct sockaddr_in6, sin6_addr), sizeof(address6));
        bytes = &address6;
    }
    CHECK(inet_ntop(address->sa_family, bytes, actual, sizeof(actual)) != NULL);
    CHECK(strcmp(actual, expected) == 0);
    CHECK(length == (address->sa_family == AF_INET ? sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6)));
}

static void set_destination(struct sockaddr_storage *destination, int family, const char *address) {
    memset(destination, 0, sizeof(*destination));
    destination->ss_family = family;
    void *bytes = family == AF_INET ? (void *)&((struct sockaddr_in *)destination)->sin_addr
                                    : (void *)&((struct sockaddr_in6 *)destination)->sin6_addr;
    CHECK(inet_pton(family, address, bytes) == 1);
}

/* Bare addresses and default prefixes (32 for IPv4, 128 for IPv6). */
static void test_parse_bare_addresses(void) {
    expect_valid("192.0.2.1", AF_INET, "192.0.2.1", 32);
    expect_valid("2001:db8::1", AF_INET6, "2001:db8::1", 128);
    expect_valid("::", AF_INET6, "::", 128);
}

/* Leading/trailing whitespace around the whole spec must be trimmed. */
static void test_parse_outer_whitespace(void) {
    expect_valid(" 192.0.2.1 ", AF_INET, "192.0.2.1", 32);
    expect_valid("\t192.0.2.1/24\t", AF_INET, "192.0.2.1", 24);
    expect_valid(" [2001:db8::1]/64 ", AF_INET6, "2001:db8::1", 64);
    expect_invalid(""); /* empty spec */
    expect_invalid("   "); /* whitespace-only spec */
}

/* Trailing whitespace after the prefix digits is accepted (regression test
 * for the prefix substring being bounded to the trimmed end, not the spec's
 * raw NUL terminator).
 */
static void test_parse_prefix_trailing_whitespace(void) {
    expect_valid("192.0.2.1/24  ", AF_INET, "192.0.2.1", 24);
    expect_valid("2001:db8::1/64\t", AF_INET6, "2001:db8::1", 64);
}

/* strtol() itself skips leading whitespace before the digits it parses, so a
 * space directly after the slash is tolerated even though embedded
 * whitespace elsewhere is not.
 */
static void test_parse_prefix_leading_whitespace_is_tolerated(void) {
    expect_valid("192.0.2.1/ 24", AF_INET, "192.0.2.1", 24);
}

/* A space between the address and the slash is part of the address text and
 * is rejected by inet_pton(), unlike whitespace around the whole spec.
 */
static void test_parse_space_before_slash_is_rejected(void) {
    expect_invalid("192.0.2.1 /24");
    expect_invalid("2001:db8::1 /64");
}

/* Bracketed IPv6 addresses, with and without a prefix, and malformed
 * bracket pairing.
 */
static void test_parse_ipv6_brackets(void) {
    expect_valid("[2001:db8::1]", AF_INET6, "2001:db8::1", 128);
    expect_valid("[2001:db8::1]/64", AF_INET6, "2001:db8::1", 64);
    expect_invalid("[2001:db8::1"); /* missing closing bracket */
    expect_invalid("2001:db8::1]"); /* missing opening bracket */
    expect_invalid("]2001:db8::1["); /* reversed brackets */
    expect_invalid("[2001:db8::1]x/64"); /* trailing garbage before the slash */
    expect_invalid("[2001:db8::1]x"); /* trailing garbage after the address */
}

/* Multiple '/' separators are rejected, with or without IPv6 brackets. */
static void test_parse_multiple_slashes(void) {
    expect_invalid("192.0.2.1/24/1");
    expect_invalid("192.0.2.1//24");
    expect_invalid("[2001:db8::1]/64/1");
}

/* Prefix-length range, sign, and malformed numeric text. */
static void test_parse_prefix_numeric_edge_cases(void) {
    expect_valid("192.0.2.1/0", AF_INET, "192.0.2.1", 0);
    expect_valid("192.0.2.1/32", AF_INET, "192.0.2.1", 32);
    expect_valid("2001:db8::1/0", AF_INET6, "2001:db8::1", 0);
    expect_valid("2001:db8::1/128", AF_INET6, "2001:db8::1", 128);
    expect_valid("192.0.2.1/024", AF_INET, "192.0.2.1", 24); /* leading zero, base 10 */

    expect_invalid("192.0.2.1/"); /* empty prefix */
    expect_invalid("192.0.2.1/33"); /* one past IPv4 max */
    expect_invalid("2001:db8::1/129"); /* one past IPv6 max */
    expect_invalid("192.0.2.1/-1"); /* negative */
    expect_invalid("192.0.2.1/24x"); /* trailing garbage after digits */
    expect_invalid("192.0.2.1/2 4"); /* embedded whitespace within digits */
    expect_invalid("192.0.2.1/99999999999999999999"); /* strtol overflow (ERANGE) */
}

/* Non-numeric hostnames/wildcards and oversized address text are rejected. */
static void test_parse_non_numeric_and_oversized(void) {
    expect_invalid("hostname.example");
    expect_invalid("*.example.com");
    expect_invalid("111111111111111111111111111111111111111111111111"); /* exceeds INET6_ADDRSTRLEN */
}

/* net_source_policy_construct() API-misuse and boundary argument handling. */
static void test_construct_argument_validation(void) {
    net_source_policy_t *policy = (net_source_policy_t *)0x1; /* deliberately non-NULL */
    CHECK(net_source_policy_construct(&policy, NULL, 0) == RS_RET_PARAM_ERROR);

    policy = NULL;
    CHECK(net_source_policy_construct(NULL, NULL, 0) == RS_RET_PARAM_ERROR);

    CHECK(net_source_policy_construct(&policy, NULL, 1) == RS_RET_PARAM_ERROR);
    CHECK(policy == NULL);

    /* Overflow guard rejects a count too large for the allocation size
     * arithmetic; it must trip before any spec is dereferenced.
     */
    const char *dummySpecs[] = {"192.0.2.1"};
    CHECK(net_source_policy_construct(&policy, dummySpecs, (size_t)-1) == RS_RET_PARAM_ERROR);
    CHECK(policy == NULL);

    CHECK(net_source_policy_construct(&policy, NULL, 0) == RS_RET_OK);
    CHECK(policy == NULL);
    CHECK(net_source_policy_count(NULL) == 0);
    CHECK(net_source_entry_address(NULL, NULL) == NULL);
}

/* A single malformed entry anywhere in the array fails the whole construct
 * call and leaves the output policy pointer NULL, even when earlier entries
 * were valid.
 */
static void test_construct_rejects_any_invalid_entry(void) {
    net_source_policy_t *policy = NULL;
    const char *trailingInvalid[] = {"192.0.2.1/24", "198.51.100.1", "192.0.2.1/33"};
    CHECK(net_source_policy_construct(&policy, trailingInvalid, 3) == RS_RET_INVALID_IP);
    CHECK(policy == NULL);

    const char *nullEntry[] = {"192.0.2.1", NULL};
    CHECK(net_source_policy_construct(&policy, nullEntry, 2) == RS_RET_INVALID_IP);
    CHECK(policy == NULL);

    const char *invalid[] = {"hostname.example", "192.0.2.1/33", "2001:db8::1/129", "192.0.2.1/24/1"};
    for (size_t index = 0; index < sizeof(invalid) / sizeof(invalid[0]); ++index) {
        CHECK(net_source_policy_construct(&policy, &invalid[index], 1) == RS_RET_INVALID_IP);
        CHECK(policy == NULL);
    }
}

/* Longest-prefix-first ranking, per family, with the fallback default-route
 * entry ranked last for a matching family.
 */
static void test_select_longest_prefix_ranking(void) {
    const char *specs[] = {"192.0.2.10/24", "198.51.100.10", "192.0.2.20/28", "2001:db8::10/32", "2001:db8:1::10/48"};
    net_source_policy_t *policy = NULL;
    CHECK(net_source_policy_construct(&policy, specs, sizeof(specs) / sizeof(specs[0])) == RS_RET_OK);
    CHECK(net_source_policy_count(policy) == 5);

    struct sockaddr_storage destination;
    set_destination(&destination, AF_INET, "192.0.2.25");
    const net_source_entry_t *entry = net_source_policy_select(policy, (const struct sockaddr *)&destination, NULL);
    check_address(entry, "192.0.2.20");
    entry = net_source_policy_select(policy, (const struct sockaddr *)&destination, entry);
    check_address(entry, "192.0.2.10");
    entry = net_source_policy_select(policy, (const struct sockaddr *)&destination, entry);
    check_address(entry, "198.51.100.10");
    CHECK(net_source_policy_select(policy, (const struct sockaddr *)&destination, entry) == NULL);

    set_destination(&destination, AF_INET6, "2001:db8:1::99");
    entry = net_source_policy_select(policy, (const struct sockaddr *)&destination, NULL);
    check_address(entry, "2001:db8:1::10");
    entry = net_source_policy_select(policy, (const struct sockaddr *)&destination, entry);
    check_address(entry, "2001:db8::10");
    CHECK(net_source_policy_select(policy, (const struct sockaddr *)&destination, entry) == NULL);

    net_source_policy_destruct(&policy);
    CHECK(policy == NULL);
}

/* Entries tied on prefix score break the tie by ascending configuration
 * order, whether or not the destination actually matches their prefix.
 */
static void test_select_configuration_order_tiebreak(void) {
    const char *tiedMatches[] = {"10.0.0.1/24", "10.0.0.2/24"};
    net_source_policy_t *policy = NULL;
    CHECK(net_source_policy_construct(&policy, tiedMatches, 2) == RS_RET_OK);

    struct sockaddr_storage destination;
    set_destination(&destination, AF_INET, "10.0.0.99");
    const net_source_entry_t *entry = net_source_policy_select(policy, (const struct sockaddr *)&destination, NULL);
    check_address(entry, "10.0.0.1");
    entry = net_source_policy_select(policy, (const struct sockaddr *)&destination, entry);
    check_address(entry, "10.0.0.2");
    CHECK(net_source_policy_select(policy, (const struct sockaddr *)&destination, entry) == NULL);
    net_source_policy_destruct(&policy);

    const char *tiedFallbacks[] = {"10.1.0.1/24", "10.2.0.2/24"};
    CHECK(net_source_policy_construct(&policy, tiedFallbacks, 2) == RS_RET_OK);
    set_destination(&destination, AF_INET, "192.0.2.1"); /* matches neither prefix */
    entry = net_source_policy_select(policy, (const struct sockaddr *)&destination, NULL);
    check_address(entry, "10.1.0.1");
    entry = net_source_policy_select(policy, (const struct sockaddr *)&destination, entry);
    check_address(entry, "10.2.0.2");
    CHECK(net_source_policy_select(policy, (const struct sockaddr *)&destination, entry) == NULL);
    net_source_policy_destruct(&policy);
}

/* A policy containing only one address family yields no candidates for a
 * destination of the other family.
 */
static void test_select_family_isolation(void) {
    const char *ipv4Only[] = {"192.0.2.1/24"};
    net_source_policy_t *policy = NULL;
    CHECK(net_source_policy_construct(&policy, ipv4Only, 1) == RS_RET_OK);

    struct sockaddr_storage destination;
    set_destination(&destination, AF_INET6, "2001:db8::1");
    CHECK(net_source_policy_select(policy, (const struct sockaddr *)&destination, NULL) == NULL);
    net_source_policy_destruct(&policy);
}

int main(void) {
    test_parse_bare_addresses();
    test_parse_outer_whitespace();
    test_parse_prefix_trailing_whitespace();
    test_parse_prefix_leading_whitespace_is_tolerated();
    test_parse_space_before_slash_is_rejected();
    test_parse_ipv6_brackets();
    test_parse_multiple_slashes();
    test_parse_prefix_numeric_edge_cases();
    test_parse_non_numeric_and_oversized();
    test_construct_argument_validation();
    test_construct_rejects_any_invalid_entry();
    test_select_longest_prefix_ranking();
    test_select_configuration_order_tiebreak();
    test_select_family_isolation();

    puts("source-address policy tests passed");
    return 0;
}
