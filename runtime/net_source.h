/**
 * @file net_source.h
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
#ifndef INCLUDED_NET_SOURCE_H
#define INCLUDED_NET_SOURCE_H

#include <stddef.h>
#include <sys/socket.h>

#include "rsyslog.h"

/**
 * @brief Construct an immutable source-address selection policy.
 * @param policy Output location, which must point to NULL on entry.
 * @param specs Array of numeric IPv4 or IPv6 address strings with optional
 *        CIDR prefixes; may be NULL when @p count is zero.
 * @param count Number of entries in @p specs.
 * @return RS_RET_OK on success, RS_RET_INVALID_IP for an invalid entry,
 *        RS_RET_OUT_OF_MEMORY on allocation failure, or RS_RET_PARAM_ERROR for
 *        an invalid API argument.
 * @details The policy copies and parses every specification. The caller owns
 *        the returned policy and must destroy it with
 *        net_source_policy_destruct(). A zero count leaves @p policy NULL.
 */
rsRetVal net_source_policy_construct(net_source_policy_t **policy, const char *const *specs, size_t count);

/**
 * @brief Destroy a source-address policy.
 * @param policy Address of the owned policy pointer; NULL is permitted.
 * @details Frees the policy and sets the caller's pointer to NULL.
 */
void net_source_policy_destruct(net_source_policy_t **policy);

/**
 * @brief Select the next best source entry for a concrete destination.
 * @param policy Immutable policy to search.
 * @param destination Destination socket address used for family and prefix
 *        matching.
 * @param after Previously returned entry, or NULL to select the first entry.
 * @return The next policy-owned entry, or NULL when no compatible entry remains.
 * @details Entries of a different address family are excluded. Matching CIDR
 *        prefixes rank longest first; ties and nonmatching same-family entries
 *        retain configuration order. The returned pointer is borrowed and
 *        remains valid only while @p policy exists.
 */
const net_source_entry_t *net_source_policy_select(const net_source_policy_t *policy,
                                                   const struct sockaddr *destination,
                                                   const net_source_entry_t *after);

/**
 * @brief Access the socket address stored by a source-policy entry.
 * @param entry Policy-owned source entry.
 * @param length Optional output location for the socket-address length.
 * @return A borrowed socket-address pointer, or NULL when @p entry is NULL.
 */
const struct sockaddr *net_source_entry_address(const net_source_entry_t *entry, socklen_t *length);

/**
 * @brief Return the number of entries in a source policy.
 * @param policy Policy to inspect, or NULL for an empty policy.
 * @return Number of configured source entries.
 */
size_t net_source_policy_count(const net_source_policy_t *policy);

#endif /* INCLUDED_NET_SOURCE_H */
