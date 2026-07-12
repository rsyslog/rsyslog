/* Net-specific parser helpers.
 *
 * Copyright 2026 Rainer Gerhards and Adiscon GmbH.
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

#ifndef INCLUDED_NET_PARSE_H
#define INCLUDED_NET_PARSE_H

#include "parse.h"
#include "net.h"

/**
 * @brief Parse an IPv4/IPv6 address or hostname wildcard with an optional bit mask.
 * @param pThis Parser object positioned at the start of the address to parse.
 * @param pIP Output pointer set to a newly allocated netAddr_t on success.
 * @param pBits Output number of prefix/mask bits (defaulted to 32 for IPv4 or
 *        128 for IPv6 when no "/bits" suffix is present).
 * @return RS_RET_OK on success, or an error code (e.g. RS_RET_INVALID_IP,
 *        RS_RET_OUT_OF_MEMORY) on failure.
 */
rsRetVal parsAddrWithBits(rsParsObj *pThis, netAddr_t **pIP, int *pBits);

#endif /* INCLUDED_NET_PARSE_H */
