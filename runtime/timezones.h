/* header for timezones.c
 *
 * Copyright 2022 Attila Lakatos and Adiscon GmbH.
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
#ifndef INCLUDED_TIMEZONES_H
#define INCLUDED_TIMEZONES_H

#include "rsconf.h"

/* timezone specific parameters*/
struct timezones_s {
    tzinfo_t *tzinfos;
    int ntzinfos;
};

void displayTimezones(rsconf_t *cnf);
void sortTimezones(rsconf_t *cnf);
void glblProcessTimezone(struct cnfobj *o);
tzinfo_t *glblFindTimezone(rsconf_t *cnf, char *id);
void freeTimezones(rsconf_t *cnf);

#endif
