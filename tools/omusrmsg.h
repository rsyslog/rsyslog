/**
 * @file omusrmsg.h
 * @brief Interface for the built-in user message output module.
 *
 * This header exposes the initialization routine for the `omusrmsg`
 * module.  The module implements delivery of log messages directly to
 * logged-in users (either individually or via "wall" style broadcast).
 *
 * File begun on 2007-07-13 by Rainer Gerhards.
 *
 * Copyright 2007-2012 Adiscon GmbH.
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
#ifndef OMUSRMSG_H_INCLUDED
#define OMUSRMSG_H_INCLUDED 1

/* prototypes */
/**
 * Initialize the omusrmsg module.
 *
 * @param[in]  iIFVersRequested  interface version requested by the core
 * @param[out] ipIFVersProvided  the interface version actually provided
 * @param[out] pQueryEtryPt      receives module entry points
 * @param[in]  pHostQueryEtryPt  callback used to query host entry points
 * @param[in]  pModInfo          pointer to the module information block
 *
 * @return rsRetVal              standard return code
 */
rsRetVal modInitUsrMsg(int iIFVersRequested __attribute__((unused)),
    int *ipIFVersProvided,
    rsRetVal (**pQueryEtryPt)(), rsRetVal (*pHostQueryEtryPt)(uchar*, rsRetVal (**)()),
    modInfo_t *pModInfo);

#endif /* #ifndef OMUSRMSG_H_INCLUDED */
/* vi:set ai:
 */
