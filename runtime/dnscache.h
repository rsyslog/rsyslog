/* Definitions for dnscache module.
 *
 * Copyright 2011-2019 Adiscon GmbH.
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

#ifndef INCLUDED_DNSCACHE_H
#define INCLUDED_DNSCACHE_H

rsRetVal dnscacheInit(void);
rsRetVal dnscacheDeinit(void);
rsRetVal ATTR_NONNULL(1, 5) dnscacheLookup(struct sockaddr_storage *const addr,
                                           prop_t **const fqdn,
                                           prop_t **const fqdnLowerCase,
                                           prop_t **const localName,
                                           prop_t **const ip);

#endif /* #ifndef INCLUDED_DNSCACHE_H */
