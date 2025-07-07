/* libgcry.h - rsyslog's guardtime support library
 *
 * Copyright 2013 Adiscon GmbH.
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
#ifndef INCLUDED_LIBCRY_COMMON_H
#define INCLUDED_LIBCRY_COMMON_H
#include <stdint.h>

/* error states */
#define RSGCRYE_EI_OPEN 1 /* error opening .encinfo file */
#define RSGCRYE_OOM 4 /* ran out of memory */

#define EIF_MAX_RECTYPE_LEN 31 /* max length of record types */
#define EIF_MAX_VALUE_LEN 1023 /* max length of value types */
#define RSGCRY_FILETYPE_NAME "rsyslog-enrcyption-info"
#define ENCINFO_SUFFIX ".encinfo"

int cryGetKeyFromFile(const char* const fn, char** const key, unsigned* const keylen);

int cryGetKeyFromProg(char* cmd, char** key, unsigned* keylen);

#endif
