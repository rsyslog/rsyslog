/**
 * @file net_source.c
 * @brief Numeric source-address selection policy.
 *
 * Copyright 2026 Cisco Systems, Inc., and/or its affiliates.
 * Copyright 2026 Adiscon GmbH.
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

#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "net_source.h"

/**
 * @brief One parsed numeric source-address/prefix entry.
 */
struct net_source_entry_s {
    struct sockaddr_storage address; /**< Parsed source address in binary form */
    socklen_t address_length; /**< Valid length of @c address */
    uint8_t prefix_length; /**< CIDR prefix length as parsed from the spec */
    size_t order; /**< Original position in the configured spec list */
};

/**
 * @brief Immutable, parsed source-address selection policy.
 * @details Allocated as a single block sized for @c count trailing entries.
 */
struct net_source_policy_s {
    size_t count; /**< Number of entries in @c entries */
    net_source_entry_t entries[]; /**< Parsed source-address entries, in configured order */
};

/**
 * @brief Parse and validate one CIDR prefix length.
 * @param text NUL-terminated decimal prefix text.
 * @param maximum Maximum prefix length for the address family.
 * @param prefix Output location for the validated prefix.
 * @return RS_RET_OK on success or RS_RET_INVALID_IP for malformed input.
 */
static rsRetVal parse_prefix(const char *text, unsigned maximum, uint8_t *prefix) {
    char *end = NULL;
    errno = 0;
    const long value = strtol(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' || value < 0 || value > (long)maximum) {
        return RS_RET_INVALID_IP;
    }
    *prefix = (uint8_t)value;
    return RS_RET_OK;
}

/**
 * @brief Parse one numeric source-address specification.
 * @param entry Output entry initialized by this function.
 * @param spec Numeric IPv4 or IPv6 address with an optional CIDR prefix.
 * @param order Zero-based configuration order used to break selection ties.
 * @return RS_RET_OK on success or RS_RET_INVALID_IP for malformed input.
 * @details IPv6 brackets are accepted. The exact host address is retained for
 *        bind(), while the prefix is stored separately for destination ranking.
 */
static rsRetVal parse_entry(net_source_entry_t *entry, const char *spec, size_t order) {
    if (spec == NULL) {
        return RS_RET_INVALID_IP;
    }

    while (isspace((unsigned char)*spec)) {
        ++spec;
    }
    const char *end = spec + strlen(spec);
    while (end > spec && isspace((unsigned char)end[-1])) {
        --end;
    }
    if (end == spec) {
        return RS_RET_INVALID_IP;
    }

    const char *slash = memchr(spec, '/', (size_t)(end - spec));
    const char *addressEnd = slash == NULL ? end : slash;
    if (memchr(addressEnd + (slash != NULL), '/', (size_t)(end - addressEnd - (slash != NULL))) != NULL) {
        return RS_RET_INVALID_IP;
    }

    if (addressEnd - spec >= 2 && spec[0] == '[' && addressEnd[-1] == ']') {
        ++spec;
        --addressEnd;
    }
    const size_t address_length = (size_t)(addressEnd - spec);
    if (address_length == 0 || address_length >= INET6_ADDRSTRLEN) {
        return RS_RET_INVALID_IP;
    }

    char addressText[INET6_ADDRSTRLEN];
    memcpy(addressText, spec, address_length);
    addressText[address_length] = '\0';

    memset(entry, 0, sizeof(*entry));
    struct sockaddr_in *ipv4 = (struct sockaddr_in *)&entry->address;
    struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *)&entry->address;
    if (inet_pton(AF_INET, addressText, &ipv4->sin_addr) == 1) {
        ipv4->sin_family = AF_INET;
        entry->address_length = sizeof(*ipv4);
        entry->prefix_length = 32;
    } else if (inet_pton(AF_INET6, addressText, &ipv6->sin6_addr) == 1) {
        ipv6->sin6_family = AF_INET6;
        entry->address_length = sizeof(*ipv6);
        entry->prefix_length = 128;
    } else {
        return RS_RET_INVALID_IP;
    }

    if (slash != NULL) {
        const unsigned maximum = ipv4->sin_family == AF_INET ? 32U : 128U;
        const size_t prefix_length = (size_t)(end - (slash + 1));
        if (prefix_length == 0 || prefix_length >= sizeof(addressText)) {
            return RS_RET_INVALID_IP;
        }
        char prefixText[sizeof(addressText)];
        memcpy(prefixText, slash + 1, prefix_length);
        prefixText[prefix_length] = '\0';
        rsRetVal result = parse_prefix(prefixText, maximum, &entry->prefix_length);
        if (result != RS_RET_OK) {
            return result;
        }
    }
    entry->order = order;
    return RS_RET_OK;
}

/**
 * @brief Construct an immutable source-address selection policy.
 * @param policy Output location, which must point to NULL on entry.
 * @param specs Array of numeric address specifications, or NULL for zero count.
 * @param count Number of entries in @p specs.
 * @return RS_RET_OK on success, RS_RET_INVALID_IP for an invalid entry,
 *        RS_RET_OUT_OF_MEMORY on allocation failure, or RS_RET_PARAM_ERROR for
 *        an invalid API argument.
 * @details All parsed entries are stored in one owned allocation. A zero count
 *        is represented by a NULL policy.
 */
rsRetVal net_source_policy_construct(net_source_policy_t **policy, const char *const *specs, size_t count) {
    if (policy == NULL || *policy != NULL || (count != 0 && specs == NULL) ||
        count > (SIZE_MAX - sizeof(net_source_policy_t)) / sizeof(net_source_entry_t)) {
        return RS_RET_PARAM_ERROR;
    }
    if (count == 0) {
        return RS_RET_OK;
    }

    net_source_policy_t *created = calloc(1, sizeof(*created) + count * sizeof(created->entries[0]));
    if (created == NULL) {
        return RS_RET_OUT_OF_MEMORY;
    }
    created->count = count;
    for (size_t index = 0; index < count; ++index) {
        const rsRetVal result = parse_entry(&created->entries[index], specs[index], index);
        if (result != RS_RET_OK) {
            free(created);
            return result;
        }
    }
    *policy = created;
    return RS_RET_OK;
}

/**
 * @brief Destroy a source-address policy.
 * @param policy Address of the owned policy pointer; NULL is permitted.
 * @details Frees the policy and sets the caller's pointer to NULL.
 */
void net_source_policy_destruct(net_source_policy_t **policy) {
    if (policy == NULL) {
        return;
    }
    free(*policy);
    *policy = NULL;
}

/**
 * @brief Score one source entry against a concrete destination.
 * @param entry Source-policy entry to score.
 * @param destination Destination socket address.
 * @return Zero for an incompatible family, one for a same-family prefix
 *        mismatch, or prefix length plus two for a matching prefix.
 */
static unsigned prefix_score(const net_source_entry_t *entry, const struct sockaddr *destination) {
    if (destination == NULL || entry->address.ss_family != destination->sa_family) {
        return 0;
    }

    const uint8_t *sourceBytes;
    const uint8_t *destinationBytes;
    struct in_addr destinationAddress;
    struct in6_addr destinationAddress6;
    if (destination->sa_family == AF_INET) {
        sourceBytes = (const uint8_t *)&((const struct sockaddr_in *)&entry->address)->sin_addr;
        memcpy(&destinationAddress, (const uint8_t *)destination + offsetof(struct sockaddr_in, sin_addr),
               sizeof(destinationAddress));
        destinationBytes = (const uint8_t *)&destinationAddress;
    } else if (destination->sa_family == AF_INET6) {
        sourceBytes = (const uint8_t *)&((const struct sockaddr_in6 *)&entry->address)->sin6_addr;
        memcpy(&destinationAddress6, (const uint8_t *)destination + offsetof(struct sockaddr_in6, sin6_addr),
               sizeof(destinationAddress6));
        destinationBytes = (const uint8_t *)&destinationAddress6;
    } else {
        return 0;
    }

    const unsigned whole_bytes = entry->prefix_length / 8;
    const unsigned remaining_bits = entry->prefix_length % 8;
    if (whole_bytes != 0 && memcmp(sourceBytes, destinationBytes, whole_bytes) != 0) {
        return 1;
    }
    if (remaining_bits != 0) {
        const uint8_t mask = (uint8_t)(0xffU << (8 - remaining_bits));
        if ((sourceBytes[whole_bytes] & mask) != (destinationBytes[whole_bytes] & mask)) {
            return 1;
        }
    }
    return (unsigned)entry->prefix_length + 2;
}

/**
 * @brief Select the next best source entry for a concrete destination.
 * @param policy Immutable policy to search.
 * @param destination Destination socket address used for ranking.
 * @param after Previously returned entry, or NULL for the first selection.
 * @return The next borrowed policy entry, or NULL when none remains.
 * @details Matching prefixes rank longest first; configuration order breaks
 *        ties and orders remaining same-family fallback entries.
 */
const net_source_entry_t *net_source_policy_select(const net_source_policy_t *policy,
                                                   const struct sockaddr *destination,
                                                   const net_source_entry_t *after) {
    if (policy == NULL || destination == NULL) {
        return NULL;
    }
    const unsigned afterScore = after == NULL ? UINT_MAX : prefix_score(after, destination);
    const size_t afterOrder = after == NULL ? 0 : after->order;
    const net_source_entry_t *selected = NULL;
    unsigned selectedScore = 0;

    for (size_t index = 0; index < policy->count; ++index) {
        const net_source_entry_t *candidate = &policy->entries[index];
        if (candidate->address.ss_family != destination->sa_family) {
            continue;
        }
        const unsigned score = prefix_score(candidate, destination);
        if (after != NULL && (score > afterScore || (score == afterScore && candidate->order <= afterOrder))) {
            continue;
        }
        if (selected == NULL || score > selectedScore ||
            (score == selectedScore && candidate->order < selected->order)) {
            selected = candidate;
            selectedScore = score;
        }
    }
    return selected;
}

/**
 * @brief Access the socket address stored by a source-policy entry.
 * @param entry Policy-owned source entry.
 * @param length Optional output location for the socket-address length.
 * @return A borrowed socket-address pointer, or NULL for a NULL entry.
 */
const struct sockaddr *net_source_entry_address(const net_source_entry_t *entry, socklen_t *length) {
    if (entry == NULL) {
        return NULL;
    }
    if (length != NULL) {
        *length = entry->address_length;
    }
    return (const struct sockaddr *)&entry->address;
}

/**
 * @brief Return the number of entries in a source policy.
 * @param policy Policy to inspect, or NULL for an empty policy.
 * @return Number of configured source entries.
 */
size_t net_source_policy_count(const net_source_policy_t *policy) {
    return policy == NULL ? 0 : policy->count;
}
