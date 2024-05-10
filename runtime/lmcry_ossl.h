/* An implementation of the cryprov interface for openssl.
 *
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
#ifndef INCLUDED_LMCRY_OSSL_H
#define INCLUDED_LMCRY_OSSL_H
#include "cryprov.h"

/* interface is defined in cryprov.h, we just implement it! */
#define lmcry_gcryCURR_IF_VERSION cryprovCURR_IF_VERSION
typedef cryprov_if_t lmcry_ossl_if_t;

/* the lmcry_ossl object */
struct lmcry_ossl_s {
	BEGINobjInstance; /* Data to implement generic object - MUST be the first data element! */
	osslctx ctx;
};
typedef struct lmcry_ossl_s lmcry_ossl_t;

/* prototypes */
PROTOTYPEObj(lmcry_ossl);

#endif /* #ifndef INCLUDED_LMCRY_GCRY_H */
