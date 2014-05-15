/* janitor.c - rsyslog's janitor
 *
 * The rsyslog janitor can be used to periodically clean out
 * resources. It was initially developed to close files that
 * were not written to for some time (omfile plugin), but has
 * a generic interface that can be used for all similar tasks.
 *
 * Module begun 2014-05-15 by Rainer Gerhards
 *
 * Copyright (C) 2014 by Rainer Gerhards and Adiscon GmbH.
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

#include "config.h"
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include "rsyslog.h"


void
janitorRun(void)
{
	dbgprintf("janitorRun() called\n");
}
