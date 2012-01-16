/* synrchonization-related stuff. In theory, that should
 * help porting to something different from pthreads.
 *
 * Copyright 2007-2012 Adiscon GmbH.
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

#include "rsyslog.h"
#include "sync.h"
#include "debug.h"


void
SyncObjInit(pthread_mutex_t **mut)
{
	*mut = (pthread_mutex_t *) MALLOC(sizeof (pthread_mutex_t));
	pthread_mutex_init(*mut, NULL);
}


/* This function destroys the mutex and also sets the mutex object
 * to NULL. While the later is not strictly necessary, it is a good
 * aid when debugging problems. As this function is not exepected to 
 * be called quite frequently, the additional overhead can well be
 * accepted. If this changes over time, setting to NULL may be
 * reconsidered. - rgerhards, 2007-11-12
 */
void
SyncObjExit(pthread_mutex_t **mut)
{
	if(*mut != NULL) {
		pthread_mutex_destroy(*mut);
		free(*mut);
		*mut = NULL;
	}
}
