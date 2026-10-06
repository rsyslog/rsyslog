/* net_parse.c
 * Net-specific parser helpers.
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

#include "config.h"

#include <assert.h>
#include <ctype.h>
#include <netdb.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "rsyslog.h"
#include "debug.h"
#include "net_parse.h"
#include "netns_gai.h"

static void freeNetAddrContent(struct NetAddr *const pIP) {
    if (pIP == NULL) {
        return;
    }

    if (F_ISSET(pIP->flags, ADDR_NAME)) {
        free(pIP->addr.HostWildcard);
    } else {
        free(pIP->addr.NetAddr);
    }
}

/*
 * Parsing routine for IPv4, IPv6 and domain name wildcards.
 *
 * Parses string in the format <addr>[/bits] where
 * addr can be a IPv4 address (e.g.: 127.0.0.1), IPv6 address (e.g.: [::1]),
 * full hostname (e.g.: localhost.localdomain) or hostname wildcard
 * (e.g.: *.localdomain).
 */
/**
 * @brief Parse an IPv4/IPv6 address or hostname wildcard with an optional bit mask.
 * @param pThis Parser object positioned at the start of the address to parse.
 * @param pIP Output pointer set to a newly allocated struct NetAddr on success.
 * @param pBits Output number of prefix/mask bits (defaulted to 32 for IPv4 or
 *        128 for IPv6 when no "/bits" suffix is present).
 * @return RS_RET_OK on success, or an error code (e.g. RS_RET_INVALID_IP,
 *        RS_RET_OUT_OF_MEMORY) on failure.
 */
rsRetVal parsAddrWithBits(rsParsObj *pThis, struct NetAddr **pIP, int *pBits) {
    register uchar *pC;
    uchar *pszIP = NULL;
    uchar *pszTmp;
    struct addrinfo hints, *res = NULL;
    cstr_t *pCStr;
    DEFiRet;

    rsCHECKVALIDOBJECT(pThis, OIDrsPars);
    assert(pIP != NULL);
    assert(pBits != NULL);

    CHKiRet(cstrConstruct(&pCStr));

    parsSkipWhitespace(pThis);
    pC = rsCStrGetBufBeg(pThis->pCStr) + pThis->iCurrPos;

    /* we parse everything until either '/', ',' or
     * whitespace. Validity will be checked down below.
     */
    while (pThis->iCurrPos < rsCStrLen(pThis->pCStr) && *pC != '/' && *pC != ',' && !isspace((int)*pC)) {
        if ((iRet = cstrAppendChar(pCStr, *pC)) != RS_RET_OK) {
            cstrDestruct(&pCStr);
            FINALIZE;
        }
        ++pThis->iCurrPos;
        ++pC;
    }

    cstrFinalize(pCStr);

    /* now we have the string and must check/convert it to
     * an NetAddr structure.
     */
    CHKiRet(cstrConvSzStrAndDestruct(&pCStr, &pszIP, 0));

    if ((*pIP = calloc(1, sizeof(struct NetAddr))) == NULL) {
        ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
    }

    if (*((char *)pszIP) == '[') {
        pszTmp = (uchar *)strchr((char *)pszIP, ']');
        if (pszTmp == NULL || pszTmp[1] != '\0') {
            free(*pIP);
            *pIP = NULL;
            ABORT_FINALIZE(RS_RET_INVALID_IP);
        }
        *pszTmp = '\0';

        memset(&hints, 0, sizeof(struct addrinfo));
        hints.ai_family = AF_INET6;
        hints.ai_flags = AI_NUMERICHOST;

        switch (netns_getaddrinfo((char *)pszIP + 1, NULL, &hints, &res, NULL)) {
            case 0:
                (*pIP)->addr.NetAddr = malloc(res->ai_addrlen);
                if ((*pIP)->addr.NetAddr == NULL) {
                    netns_freeaddrinfo(res);
                    free(*pIP);
                    *pIP = NULL;
                    ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
                }
                memcpy((*pIP)->addr.NetAddr, res->ai_addr, res->ai_addrlen);
                netns_freeaddrinfo(res);
                break;
            case EAI_NONAME:
                /* The "address" is not an IP prefix but a wildcard */
                F_SET((*pIP)->flags, ADDR_NAME | ADDR_PRI6);
                (*pIP)->addr.HostWildcard = strdup((const char *)pszIP + 1);
                if ((*pIP)->addr.HostWildcard == NULL) {
                    free(*pIP);
                    *pIP = NULL;
                    ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
                }
                break;
            default:
                netns_freeaddrinfo(res);
                free(*pIP);
                *pIP = NULL;
                ABORT_FINALIZE(RS_RET_ERR);
        }

        if (*pC == '/') {
            /* mask bits follow, let's parse them! */
            ++pThis->iCurrPos; /* eat slash */
            if ((iRet = parsInt(pThis, pBits)) != RS_RET_OK) {
                freeNetAddrContent(*pIP);
                free(*pIP);
                *pIP = NULL;
                FINALIZE;
            }
            /* we need to refresh pointer (changed by parsInt()) */
            pC = rsCStrGetBufBeg(pThis->pCStr) + pThis->iCurrPos;
        } else {
            /* no slash, so we assume a single host (/128) */
            *pBits = 128;
        }
    } else { /* now parse IPv4 */
        memset(&hints, 0, sizeof(struct addrinfo));
        hints.ai_family = AF_INET;
        hints.ai_flags = AI_NUMERICHOST;

        switch (netns_getaddrinfo((char *)pszIP, NULL, &hints, &res, NULL)) {
            case 0:
                (*pIP)->addr.NetAddr = malloc(res->ai_addrlen);
                if ((*pIP)->addr.NetAddr == NULL) {
                    netns_freeaddrinfo(res);
                    free(*pIP);
                    *pIP = NULL;
                    ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
                }
                memcpy((*pIP)->addr.NetAddr, res->ai_addr, res->ai_addrlen);
                netns_freeaddrinfo(res);
                break;
            case EAI_NONAME:
                /* The "address" is not an IP prefix but a wildcard */
                F_SET((*pIP)->flags, ADDR_NAME);
                (*pIP)->addr.HostWildcard = strdup((const char *)pszIP);
                if ((*pIP)->addr.HostWildcard == NULL) {
                    free(*pIP);
                    *pIP = NULL;
                    ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
                }
                break;
            default:
                netns_freeaddrinfo(res);
                free(*pIP);
                *pIP = NULL;
                ABORT_FINALIZE(RS_RET_ERR);
        }

        if (*pC == '/') {
            /* mask bits follow, let's parse them! */
            ++pThis->iCurrPos; /* eat slash */
            if ((iRet = parsInt(pThis, pBits)) != RS_RET_OK) {
                freeNetAddrContent(*pIP);
                free(*pIP);
                *pIP = NULL;
                FINALIZE;
            }
            /* we need to refresh pointer (changed by parsInt()) */
            pC = rsCStrGetBufBeg(pThis->pCStr) + pThis->iCurrPos;
        } else {
            /* no slash, so we assume a single host (/32) */
            *pBits = 32;
        }
    }

    /* skip to next processable character */
    while (pThis->iCurrPos < rsCStrLen(pThis->pCStr) && (*pC == ',' || isspace((int)*pC))) {
        ++pThis->iCurrPos;
        ++pC;
    }

    iRet = RS_RET_OK;

finalize_it:
    free(pszIP);
    RETiRet;
}
