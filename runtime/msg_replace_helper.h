/* msg_replace_helper.h
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
#ifndef INCLUDED_MSG_REPLACE_HELPER_H
#define INCLUDED_MSG_REPLACE_HELPER_H

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "rsyslog.h"

static rsRetVal msgReplaceRawMsgSegment(uchar **ppRawMsg,
                                        uchar *stackBuf,
                                        int stackBufSize,
                                        int offMSG,
                                        int lenOldMSG,
                                        int *pLenRawMsg,
                                        const uchar *pszMSG,
                                        int lenMSG) {
    int lenNew;
    int lenSuffix;
    uchar *bufNew;

    assert(ppRawMsg != NULL);
    assert(*ppRawMsg != NULL);
    assert(stackBuf != NULL);
    assert(pLenRawMsg != NULL);
    assert(pszMSG != NULL);
    assert(offMSG >= 0);
    assert(lenOldMSG >= 0);
    assert(*pLenRawMsg >= offMSG + lenOldMSG);

    lenNew = *pLenRawMsg + lenMSG - lenOldMSG;
    lenSuffix = *pLenRawMsg - offMSG - lenOldMSG;
    bufNew = *ppRawMsg;

    if (lenMSG > lenOldMSG &&
        ((bufNew == stackBuf && lenNew >= stackBufSize) || (bufNew != stackBuf && lenNew > *pLenRawMsg))) {
        if (bufNew == stackBuf) {
            bufNew = malloc(lenNew + 1);
            if (bufNew == NULL) {
                return RS_RET_OUT_OF_MEMORY;
            }
            memcpy(bufNew, *ppRawMsg, offMSG);
            if (lenSuffix > 0) {
                memcpy(bufNew + offMSG + lenMSG, *ppRawMsg + offMSG + lenOldMSG, lenSuffix);
            }
        } else {
            uchar *tmp = realloc(bufNew, lenNew + 1);

            if (tmp == NULL) {
                return RS_RET_OUT_OF_MEMORY;
            }
            bufNew = tmp;
            if (lenSuffix > 0) {
                memmove(bufNew + offMSG + lenMSG, bufNew + offMSG + lenOldMSG, lenSuffix);
            }
        }
    } else if (lenSuffix > 0 && lenMSG != lenOldMSG) {
        memmove(bufNew + offMSG + lenMSG, bufNew + offMSG + lenOldMSG, lenSuffix);
    }

    if (lenMSG > 0) {
        memcpy(bufNew + offMSG, pszMSG, lenMSG);
    }
    bufNew[lenNew] = '\0';
    *ppRawMsg = bufNew;
    *pLenRawMsg = lenNew;
    return RS_RET_OK;
}

#endif
