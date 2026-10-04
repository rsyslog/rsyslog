/* parser.c
 * This module contains functions for message parsers. It still needs to be
 * converted into an object (and much extended).
 *
 * Module begun 2008-10-09 by Rainer Gerhards (based on previous code from syslogd.c)
 *
 * Copyright 2008-2021 Rainer Gerhards and Adiscon GmbH.
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
 * tests. Caller supplies glbl and runConf. No exported API or independent runtime object.
 */
#ifndef PARSER_SANITIZE_HELPER_H_INCLUDED
#define PARSER_SANITIZE_HELPER_H_INCLUDED

/* sanitize a received message
 * if a message gets to large during sanitization, it is truncated. This is
 * as specified in the upcoming syslog RFC series.
 * rgerhards, 2008-10-09
 * We check if we have a NUL character at the very end of the
 * message. This seems to be a frequent problem with a number of senders.
 * So I have now decided to drop these NULs. However, if they are intentional,
 * that may cause us some problems, e.g. with syslog-sign. On the other hand,
 * current code always has problems with intentional NULs (as it needs to escape
 * them to prevent problems with the C string libraries), so that does not
 * really matter. Just to be on the save side, we'll log destruction of such
 * NULs in the debug log.
 * rgerhards, 2007-09-14
 */
static rsRetVal SanitizeMsg(smsg_t *pMsg) {
    static const char hexdigit[16] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};
    DEFiRet;
    uchar *pszMsg;
    uchar *pDst; /* destination for copy job */
    size_t lenMsg;
    size_t iSrc;
    size_t iDst;
    size_t iMaxLine;
    size_t maxDest;
    size_t iFirstSanitize;
    uchar pc;
    sbool bUpdatedLen = RSFALSE;
    uchar szSanBuf[32 * 1024]; /* buffer used for sanitizing a string */

    assert(pMsg != NULL);
    assert(pMsg->iLenRawMsg > 0);

    /* Reload publishes these scalars independently. Sample each setting at
     * most once before first use and retain scan settings through rewrite.
     * Rewrite-only settings are first sampled at rewrite entry below. These
     * local samples do not promise one atomic global generation.
     */
    const int dropTrailingLF = glbl.GetParserDropTrailingLFOnReception(runConf);
    const int dropTrailingCR = glbl.GetParserDropTrailingCROnReception(runConf);
    const int spaceLF = glbl.GetParserSpaceLFOnReceive(runConf);
    const int escapeControl = glbl.GetParserEscapeControlCharactersOnReceive(runConf);
    const int escape8Bit = glbl.GetParserEscape8BitCharactersOnReceive(runConf);

    pszMsg = pMsg->pszRawMsg;
    lenMsg = pMsg->iLenRawMsg;

    /* remove NUL character at end of message (see comment in function header)
     * Note that we do not need to add a NUL character in this case, because it
     * is already present ;)
     */
    if (pszMsg[lenMsg - 1] == '\0') {
        DBGPRINTF("dropped NUL at very end of message\n");
        bUpdatedLen = RSTRUE;
        lenMsg--;
    }

    /* then we check if we need to drop trailing LFs, which often make
     * their way into syslog messages unintentionally. In order to remain
     * compatible to recent IETF developments, we allow the user to
     * turn on/off this handling.  rgerhards, 2007-07-23
     */
    if (dropTrailingLF && lenMsg > 0 && pszMsg[lenMsg - 1] == '\n') {
        DBGPRINTF("dropped LF at very end of message (DropTrailingLF is set)\n");
        lenMsg--;
        pszMsg[lenMsg] = '\0';
        bUpdatedLen = RSTRUE;
    }

    if (dropTrailingCR && lenMsg > 0 && pszMsg[lenMsg - 1] == '\r') {
        DBGPRINTF("dropped CR at very end of message (DropTrailingCR is set)\n");
        lenMsg--;
        pszMsg[lenMsg] = '\0';
        bUpdatedLen = RSTRUE;
    }

    /* it is much quicker to sweep over the message and see if it actually
     * needs sanitation than to do the sanitation in any case. So we first do
     * this and terminate when it is not needed - which is expectedly the case
     * for the vast majority of messages. -- rgerhards, 2009-06-15
     * Note that we do NOT check here if tab characters are to be escaped or
     * not. I expect this functionality to be seldomly used and thus I do not
     * like to pay the performance penalty. So the penalty is only with those
     * that actually use it, because we may call the sanitizer without actual
     * need below (but it then still will work perfectly well!). -- rgerhards, 2009-11-27
     */
    int bNeedSanitize = 0;
    iFirstSanitize = lenMsg;
    for (iSrc = 0; iSrc < lenMsg; iSrc++) {
        if (pszMsg[iSrc] < 32) {
            if (spaceLF && pszMsg[iSrc] == '\n') {
                pszMsg[iSrc] = ' ';
            } else if (pszMsg[iSrc] == '\0' || escapeControl) {
                bNeedSanitize = 1;
                if (iFirstSanitize == lenMsg) iFirstSanitize = iSrc;
                if (!spaceLF) {
                    break;
                }
            }
        } else if (pszMsg[iSrc] > 127 && escape8Bit) {
            bNeedSanitize = 1;
            if (iFirstSanitize == lenMsg) iFirstSanitize = iSrc;
            if (!spaceLF) {
                break;
            }
        }
    }

    if (!bNeedSanitize) {
        if (bUpdatedLen == RSTRUE) MsgSetRawMsgSize(pMsg, lenMsg);
        FINALIZE;
    }
    /* These settings are rewrite-only; leave the common ASCII path cheap. */
    const int escapeTab = glbl.GetParserEscapeControlCharacterTab(runConf);
    const int escapeCStyle = glbl.GetParserEscapeControlCharactersCStyle(runConf);
    const uchar escapePrefix = glbl.GetParserControlCharacterEscapePrefix(runConf);
    const size_t maxLine = glbl.GetMaxLine(runConf);
    iSrc = iFirstSanitize;

    /* now copy over the message and sanitize it. Note that up to iSrc-1 there was
     * obviously no need to sanitize, so we can go over that quickly...
     */
    iMaxLine = maxLine;
    maxDest = lenMsg * 4; /* message can grow at most four-fold */

    if (maxDest > iMaxLine) maxDest = iMaxLine; /* but not more than the max size! */
    if (maxDest < sizeof(szSanBuf))
        pDst = szSanBuf;
    else
        CHKmalloc(pDst = malloc(maxDest + 1));
    if (iSrc > 0) {
        iSrc--; /* go back to where everything is OK */
        if (iSrc > maxDest) {
            DBGPRINTF(
                "parser.Sanitize: have oversize index %zd, "
                "max %zd - corrected, but should not happen\n",
                iSrc, maxDest);
            iSrc = maxDest;
        }
        memcpy(pDst, pszMsg, iSrc); /* fast copy known good */
    }
    iDst = iSrc;
    while (iSrc < lenMsg && iDst < maxDest - 3) { /* leave some space if last char must be escaped */
        if ((pszMsg[iSrc] < 32) && (pszMsg[iSrc] != '\t' || escapeTab)) {
            if (spaceLF && pszMsg[iSrc] == '\n') {
                pDst[iDst++] = ' ';
                iSrc++;
                continue;
            }
            /* note: \0 must always be escaped, the rest of the code currently
             * can not handle it! -- rgerhards, 2009-08-26
             */
            if (pszMsg[iSrc] == '\0' || escapeControl) {
                /* we are configured to escape control characters. Please note
                 * that this most probably break non-western character sets like
                 * Japanese, Korean or Chinese. rgerhards, 2007-07-17
                 */
                if (escapeCStyle) {
                    pDst[iDst++] = '\\';

                    switch (pszMsg[iSrc]) {
                        case '\0':
                            pDst[iDst++] = '0';
                            break;
                        case '\a':
                            pDst[iDst++] = 'a';
                            break;
                        case '\b':
                            pDst[iDst++] = 'b';
                            break;
                        case '\x1b': /* equivalent to '\e' which is not accepted by XLC */
                            pDst[iDst++] = 'e';
                            break;
                        case '\f':
                            pDst[iDst++] = 'f';
                            break;
                        case '\n':
                            pDst[iDst++] = 'n';
                            break;
                        case '\r':
                            pDst[iDst++] = 'r';
                            break;
                        case '\t':
                            pDst[iDst++] = 't';
                            break;
                        case '\v':
                            pDst[iDst++] = 'v';
                            break;
                        default:
                            pDst[iDst++] = 'x';

                            pc = pszMsg[iSrc];
                            pDst[iDst++] = hexdigit[(pc & 0xF0) >> 4];
                            pDst[iDst++] = hexdigit[pc & 0xF];

                            break;
                    }

                } else {
                    pDst[iDst++] = escapePrefix;
                    pDst[iDst++] = '0' + ((pszMsg[iSrc] & 0300) >> 6);
                    pDst[iDst++] = '0' + ((pszMsg[iSrc] & 0070) >> 3);
                    pDst[iDst++] = '0' + ((pszMsg[iSrc] & 0007));
                }
            }

        } else if (pszMsg[iSrc] > 127 && escape8Bit) {
            if (escapeCStyle) {
                pDst[iDst++] = '\\';
                pDst[iDst++] = 'x';

                pc = pszMsg[iSrc];
                pDst[iDst++] = hexdigit[(pc & 0xF0) >> 4];
                pDst[iDst++] = hexdigit[pc & 0xF];

            } else {
                /* In this case, we also do the conversion. Note that this most
                 * probably breaks European languages. -- rgerhards, 2010-01-27
                 */
                pDst[iDst++] = escapePrefix;
                pDst[iDst++] = '0' + ((pszMsg[iSrc] & 0300) >> 6);
                pDst[iDst++] = '0' + ((pszMsg[iSrc] & 0070) >> 3);
                pDst[iDst++] = '0' + ((pszMsg[iSrc] & 0007));
            }
        } else {
            pDst[iDst++] = pszMsg[iSrc];
        }
        ++iSrc;
    }
    pDst[iDst] = '\0';

    MsgSetRawMsg(pMsg, (char *)pDst, iDst); /* save sanitized string */

    if (pDst != szSanBuf) free(pDst);

finalize_it:
    RETiRet;
}

#endif
