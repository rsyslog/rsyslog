/* OperatingStateFile interface.
 *
 * Copyright 2018 Rainer Gerhards and Adiscon GmbH.
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
#ifndef INCLUDED_OPERATINGSTATEFILE_H
#define INCLUDED_OPERATINGSTATEFILE_H

/* supported TAGS */
#define OSF_TAG_STATE   "STATE"
#define OSF_TAG_MSG "MSG"

void osf_open(void);
void ATTR_NONNULL() osf_write(const char *const tag, const char *const line);
void osf_close(void);

#endif /* #ifndef INCLUDED_OPERATINGSTATEFILE_H */
