/* atomic_posix_sem.c: This file supplies an emulation for atomic operations using
 * POSIX semaphores.
 *
 * Copyright 2010 DResearch Digital Media Systems GmbH
 *
 * This file is part of the rsyslog runtime library.
 *
 * The rsyslog runtime library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * The rsyslog runtime library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with the rsyslog runtime library.  If not, see <http://www.gnu.org/licenses/>.
 *
 * A copy of the GPL can be found in the file "COPYING" in this distribution.
 * A copy of the LGPL can be found in the file "COPYING.LESSER" in this distribution.
 */

#include "config.h"
#ifndef HAVE_ATOMIC_BUILTINS
#ifdef HAVE_SEMAPHORE_H
#include <semaphore.h>
#include <errno.h>

#include "atomic.h"
#include "rsyslog.h"
#include "srUtils.h"

sem_t atomicSem;

rsRetVal
atomicSemInit(void)
{
	DEFiRet;

	dbgprintf("init posix semaphore for atomics emulation\n");
	if(sem_init(&atomicSem, 0, 1) == -1)
	{
		char errStr[1024];
		rs_strerror_r(errno, errStr, sizeof(errStr));
		dbgprintf("init posix semaphore for atomics emulation failed: %s\n", errStr);
		iRet = RS_RET_SYS_ERR; /* the right error code ??? */
	}

	RETiRet;
}

void
atomicSemExit(void)
{
	dbgprintf("destroy posix semaphore for atomics emulation\n");
	if(sem_destroy(&atomicSem) == -1)
	{
		char errStr[1024];
		rs_strerror_r(errno, errStr, sizeof(errStr));
		dbgprintf("destroy posix semaphore for atomics emulation failed: %s\n", errStr);
	}
}

#endif /* HAVE_SEMAPHORE_H */
#endif /* !defined(HAVE_ATOMIC_BUILTINS) */

/* vim:set ai:
 */
