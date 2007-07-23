/* modules.c
 * This is the implementation of syslogd modules object.
 * This object handles plug-ins and buil-in modules of all kind.
 *
 * File begun on 2007-07-22 by RGerhards
 *
 * Copyright 2007 Rainer Gerhards and Adiscon GmbH.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * A copy of the GPL can be found in the file "COPYING" in this distribution.
 */
#include "config.h"
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <assert.h>
#include <errno.h>

#include <unistd.h>
#include <sys/file.h>

#include "rsyslog.h"
#include "modules.h"

static modInfo_t *pLoadedModules = NULL;	/* list of currently-loaded modules */
static modInfo_t *pLoadedModulesLast = NULL; /* tail-pointer */


/* Construct a new module object
 */
static rsRetVal moduleConstruct(modInfo_t **pThis)
{
	modInfo_t *pNew;

	if((pNew = calloc(1, sizeof(modInfo_t))) == NULL)
		return RS_RET_OUT_OF_MEMORY;

	/* OK, we got the element, now initialize members that should
	 * not be zero-filled.
	 */

	*pThis = pNew;
	return RS_RET_OK;
}


/* Add a module to the loaded module linked list
 */
static inline void addModToList(modInfo_t *pThis)
{
	assert(pThis != NULL);

	if(pLoadedModules == NULL) {
		pLoadedModules = pLoadedModulesLast = pThis;
	} else {
		/* there already exist entries */
		pLoadedModulesLast->pNext = pThis;
		pLoadedModulesLast = pThis;
	}
}


/* Add an already-loaded module to the module linked list. This function does
 * anything that is needed to fully initialize the module.
 */
rsRetVal doModInit(rsRetVal *doInit(), uchar *name)
{
	modInfo_t *pNew;
	rsRetVal iRet;

	if((iRet = moduleConstruct(&pNew)) != RS_RET_OK)
		return iRet;

	if((iRet = *doInit()) != RS_RET_OK)
		return iRet;

	/* OK, we know we can successfully work with the module. So we now fill the
	 * rest of the data elements.
	 */

	pNew->pszModName = (uchar*) strdup((char*)name); /* we do not care if strdup() fails, we can accept that */

	return RS_RET_OK;
}
/*
 * vi:set ai:
 */
