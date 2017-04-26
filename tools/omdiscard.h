/* omdiscard.h
 * These are the definitions for the built-in discard output module.
 *
 * File begun on 2007-07-24 by RGerhards
 *
 * Copyright 2007-2012 Rainer Gerhards and Adiscon GmbH.
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
#ifndef OMDISCARD_H_INCLUDED
#define OMDISCARD_H_INCLUDED 1

/* prototypes */
rsRetVal modInitDiscard(int iIFVersRequested __attribute__((unused)), int *ipIFVersProvided,
    rsRetVal (**pQueryEtryPt)(), rsRetVal (*pHostQueryEtryPt)(uchar *, rsRetVal (**)()), modInfo_t *);

#endif /* #ifndef OMDISCARD_H_INCLUDED */
/* vi:set ai:
 */
