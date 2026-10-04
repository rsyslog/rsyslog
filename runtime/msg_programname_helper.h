/* msg.c
 * The msg object. Implementation of all msg-related functions
 *
 * File begun on 2007-07-13 by RGerhards (extracted from syslogd.c)
 * This file is under development and has not yet arrived at being fully
 * self-contained and a real object. So far, it is mostly an excerpt
 * of the "old" message code without any modifications. However, it
 * helps to have things at the right place one we go to the meat of it.
 *
 * Copyright 2007-2026 Rainer Gerhards and Adiscon GmbH.
 *
 * This file is part of the rsyslog runtime library.
 *
 * The rsyslog runtime library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * The rsyslog runtime library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with the rsyslog runtime library.  If not, see <http://www.gnu.org/licenses/>.
 *
 * A copy of the GPL can be found in the file "COPYING" in this distribution.
 * A copy of the LGPL can be found in the file "COPYING.LESSER" in this distribution.
 */


/* Private implementation shared only by its runtime owner and direct unit
 * tests. Caller supplies runConf. No exported API or independent runtime object.
 */
#ifndef MSG_PROGRAMNAME_HELPER_H_INCLUDED
#define MSG_PROGRAMNAME_HELPER_H_INCLUDED

/* Parse and set the "programname" for a given MSG object. Programname
 * is a BSD concept, it is the tag without any instance-specific information.
 * Precisely, the programname is terminated by either (whichever occurs first):
 * - end of tag
 * - nonprintable character
 * - ':'
 * - '['
 * - '/'
 * The above definition has been taken from the FreeBSD syslogd sources.
 *
 * The program name is not parsed by default, because it is infrequently-used.
 * IMPORTANT: A locked message object must be provided, else a crash will occur.
 * rgerhards, 2005-10-19
 */
static rsRetVal acquireProgramName(smsg_t *const pM) {
    int i;
    uchar *pszTag, *pszProgName;
    DEFiRet;

    assert(pM != NULL);
    pszTag = (uchar *)((pM->iLenTAG < CONF_TAG_BUFSIZE) ? pM->TAG.szBuf : pM->TAG.pszTAG);
    /* Keep this TAG scan on one policy even if reload publishes a new scalar.
     * This does not bind lazy property evaluation to a message generation.
     */
    const int permitSlash = glblGetParserPermitSlashInProgramName(runConf);
    for (i = 0; (i < pM->iLenTAG) && isprint((int)pszTag[i]) && (pszTag[i] != '\0') && (pszTag[i] != ':') &&
                (pszTag[i] != '[') && (permitSlash || (pszTag[i] != '/'));
         ++i); /* just search end of PROGNAME */
    if (i < CONF_PROGNAME_BUFSIZE) {
        pszProgName = pM->PROGNAME.szBuf;
    } else {
        CHKmalloc(pM->PROGNAME.ptr = malloc(i + 1));
        pszProgName = pM->PROGNAME.ptr;
    }
    memcpy((char *)pszProgName, (char *)pszTag, i);
    pszProgName[i] = '\0';
    pM->iLenPROGNAME = i;
finalize_it:
    RETiRet;
}

#endif
