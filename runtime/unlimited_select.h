/* unlimited_select.h
 * Tweak the macros for accessing fd_set so that the select() syscall
 * won't be limited to a particular number of file descriptors.
 *
 * Copyright 2009-2012 Adiscon GmbH.
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

#ifndef	UNLIMITED_SELECT_H_INCLUDED
#define UNLIMITED_SELECT_H_INCLUDED

#include <string.h>
#include <stdlib.h>
#include <sys/select.h>
#include "glbl.h"

#ifdef USE_UNLIMITED_SELECT
# undef FD_ZERO
# define FD_ZERO(set) memset((set), 0, glbl.GetFdSetSize());
#endif

#ifdef USE_UNLIMITED_SELECT
static inline void freeFdSet(fd_set *p) {
	free(p);
}
#else
#	define freeFdSet(x)
#endif

#endif /* #ifndef UNLIMITED_SELECT_H_INCLUDED */
