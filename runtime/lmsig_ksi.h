/* An implementation of the sigprov interface for KSI.
 *
 * Copyright 2013-2015 Adiscon GmbH.
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
#ifndef INCLUDED_LMSIG_KSI_H
#define INCLUDED_LMSIG_KSI_H
#include "sigprov.h"
#include "librsgt_common.h"
#include "librsksi.h"

/* interface is defined in sigprov.h, we just implement it! */
#define lmsig_ksiCURR_IF_VERSION sigprovCURR_IF_VERSION
typedef sigprov_if_t lmsig_ksi_if_t;

/* the lmsig_ksi object */
struct lmsig_ksi_s {
	BEGINobjInstance; /* Data to implement generic object - MUST be the first data element! */
	rsksictx ctx;	/* librsksi context - contains all we need */
};
typedef struct lmsig_ksi_s lmsig_ksi_t;

/* prototypes */
PROTOTYPEObj(lmsig_ksi);

#endif /* #ifndef INCLUDED_LMSIG_KSI_H */
