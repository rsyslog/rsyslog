/* The zlibwrap object.
 *
 * This is an rsyslog object wrapper around zlib.
 *
 * Copyright 2009 Rainer Gerhards and Adiscon GmbH.
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
#include <string.h>
#include <assert.h>
#include <zlib.h>

#include "rsyslog.h"
#include "module-template.h"
#include "obj.h"
#include "zlibw.h"

MODULE_TYPE_LIB
MODULE_TYPE_NOKEEP

/* static data */
DEFobjStaticHelpers


/* ------------------------------ methods ------------------------------ */

/* zlib make strong use of macros for its interface functions, so we can not simply
 * pass function pointers to them. Instead, we create very small wrappers which call
 * the relevant entry points.
 */

static int myDeflateInit(z_streamp strm, int level)
{
	return deflateInit(strm, level);
}

static int myDeflateInit2(z_streamp strm, int level, int method, int windowBits, int memLevel, int strategy)
{
	return deflateInit2(strm, level, method, windowBits, memLevel, strategy);
}

static int myDeflateEnd(z_streamp strm)
{
	return deflateEnd(strm);
}

static int myDeflate(z_streamp strm, int flush)
{
	return deflate(strm, flush);
}


/* queryInterface function
 * rgerhards, 2008-03-05
 */
BEGINobjQueryInterface(zlibw)
CODESTARTobjQueryInterface(zlibw)
	if(pIf->ifVersion != zlibwCURR_IF_VERSION) { /* check for current version, increment on each change */
		ABORT_FINALIZE(RS_RET_INTERFACE_NOT_SUPPORTED);
	}

	/* ok, we have the right interface, so let's fill it
	 * Please note that we may also do some backwards-compatibility
	 * work here (if we can support an older interface version - that,
	 * of course, also affects the "if" above).
	 */
	pIf->DeflateInit = myDeflateInit;
	pIf->DeflateInit2 = myDeflateInit2;
	pIf->Deflate     = myDeflate;
	pIf->DeflateEnd  = myDeflateEnd;
finalize_it:
ENDobjQueryInterface(zlibw)


/* Initialize the zlibw class. Must be called as the very first method
 * before anything else is called inside this class.
 * rgerhards, 2008-02-19
 */
BEGINAbstractObjClassInit(zlibw, 1, OBJ_IS_LOADABLE_MODULE) /* class, version */
	/* request objects we use */

	/* set our own handlers */
ENDObjClassInit(zlibw)


/* --------------- here now comes the plumbing that makes as a library module --------------- */


BEGINmodExit
CODESTARTmodExit
ENDmodExit


BEGINqueryEtryPt
CODESTARTqueryEtryPt
CODEqueryEtryPt_STD_LIB_QUERIES
ENDqueryEtryPt


BEGINmodInit()
CODESTARTmodInit
	*ipIFVersProvided = CURR_MOD_IF_VERSION; /* we only support the current interface specification */

	CHKiRet(zlibwClassInit(pModInfo)); /* must be done after tcps_sess, as we use it */
	/* Initialize all classes that are in our module - this includes ourselfs */
ENDmodInit
/* vi:set ai:
 */
