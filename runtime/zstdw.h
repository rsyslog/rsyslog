/* The zstdw object. It encapsulates the zstd functionality. The primary
 * purpose of this wrapper class is to enable rsyslogd core to be build without
 * zstd libraries.
 *
 * Copyright 2022 Adiscon GmbH.
 *
 * This file is part of the rsyslog runtime library.
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
#ifndef INCLUDED_ZSTDW_H
#define INCLUDED_ZSTDW_H

/* interfaces */
BEGINinterface(zstdw) /* name must also be changed in ENDinterface macro! */
    rsRetVal (*doStrmWrite)(strm_t *pThis, uchar *const pBuf, const size_t lenBuf, const int bFlush,
                            rsRetVal (*strmPhysWrite)(strm_t *pThis, uchar *pBuf, size_t lenBuf));
    rsRetVal (*doCompressFinish)(strm_t *pThis, rsRetVal (*Destruct)(strm_t *pThis, uchar *pBuf, size_t lenBuf));
    rsRetVal (*Destruct)(strm_t *pThis);
ENDinterface(zstdw)
#define zstdwCURR_IF_VERSION 1 /* increment whenever you change the interface structure! */


/* prototypes */
PROTOTYPEObj(zstdw);

/* the name of our library binary */
#define LM_ZSTDW_FILENAME "lmzstdw"

#endif /* #ifndef INCLUDED_ZSTDW_H */
