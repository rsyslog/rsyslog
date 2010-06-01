/* smtradfile.c
 * This is a strgen module for the traditional file format.
 *
 * NOTE: read comments in module-template.h to understand how this file
 *       works!
 *
 * File begun on 2010-06-01 by RGerhards
 *
 * Copyright 2010 Rainer Gerhards and Adiscon GmbH.
 *
 * This file is part of rsyslog.
 *
 * Rsyslog is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Rsyslog is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Rsyslog.  If not, see <http://www.gnu.org/licenses/>.
 *
 * A copy of the GPL can be found in the file "COPYING" in this distribution.
 */
#include "config.h"
#include "rsyslog.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include "syslogd.h"
#include "conf.h"
#include "syslogd-types.h"
#include "template.h"
#include "msg.h"
#include "module-template.h"
#include "glbl.h"
#include "errmsg.h"
#include "parser.h"
#include "datetime.h"
#include "unicode-helper.h"

MODULE_TYPE_STRGEN
STRGEN_NAME("RSYSLOG_TraditionalFileFormat")

/* internal structures
 */
DEF_SMOD_STATIC_DATA
DEFobjCurrIf(errmsg)
DEFobjCurrIf(glbl)
DEFobjCurrIf(parser)
DEFobjCurrIf(datetime)


/* config data */


#warning TODO: ExtendBuf via object interface!
extern rsRetVal ExtendBuf(uchar **pBuf, size_t *pLenBuf, size_t iMinSize);
extern char *getTimeReported(msg_t *pM, enum tplFormatTypes eFmt);

BEGINstrgen
	int iBuf;
	uchar *pVal;
	size_t iLenVal;
CODESTARTstrgen
	dbgprintf("XXX: strgen module, *ppBuf = %p\n", *ppBuf);
	iBuf = 0;
	dbgprintf("TIMESTAMP\n");
	/* TIMESTAMP + ' ' */
	dbgprintf("getTimeReported\n");
	pVal = (uchar*) getTimeReported(pMsg, tplFmtDefault);
	dbgprintf("obtain iLenVal ptr %p\n", pVal);
	iLenVal = ustrlen(pVal);
	dbgprintf("TIMESTAMP pVal='%p', iLenVal=%d\n", pVal, iLenVal);
	if(iBuf + iLenVal + 1 >= *pLenBuf) /* we reserve one char for the final \0! */
		CHKiRet(ExtendBuf(ppBuf, pLenBuf, iBuf + iLenVal + 1));
	memcpy(*ppBuf + iBuf, pVal, iLenVal);
	iBuf += iLenVal;
	*(*ppBuf + iBuf++) = ' ';

	dbgprintf("HOSTNAME\n");
	/* HOSTNAME + ' ' */
	pVal = (uchar*) getHOSTNAME(pMsg);
	iLenVal = getHOSTNAMELen(pMsg);
	if(iBuf + iLenVal + 1 >= *pLenBuf) /* we reserve one char for the ' '! */
		CHKiRet(ExtendBuf(ppBuf, pLenBuf, iBuf + iLenVal + 1));
	memcpy(*ppBuf + iBuf, pVal, iLenVal);
	iBuf += iLenVal;
	*(*ppBuf + iBuf++) = ' ';

	dbgprintf("TAG\n");
	/* syslogtag */
	/* max size for TAG assumed 200 * TODO: check! */
	if(iBuf + 200 >= *pLenBuf)
		CHKiRet(ExtendBuf(ppBuf, pLenBuf, iBuf + 200));
	getTAG(pMsg, &pVal, &iLenVal);
	memcpy(*ppBuf + iBuf, pVal, iLenVal);
	iBuf += iLenVal;

	dbgprintf("MSG\n");
	/* MSG, plus leading space if necessary */
	pVal = getMSG(pMsg);
	iLenVal = getMSGLen(pMsg);
	if(iBuf + iLenVal + 1 >= *pLenBuf) /* we reserve one char for the leading SP*/
		CHKiRet(ExtendBuf(ppBuf, pLenBuf, iBuf + iLenVal + 1));
	if(pVal[0] != ' ')
		*(*ppBuf + iBuf++) = ' ';
	memcpy(*ppBuf + iBuf, pVal, iLenVal);
	iBuf += iLenVal;

	dbgprintf("Trailer\n");
	/* end sequence */
	iLenVal = 2;
	if(iBuf + iLenVal  >= *pLenBuf) /* we reserve one char for the final \0! */
		CHKiRet(ExtendBuf(ppBuf, pLenBuf, iBuf + iLenVal + 1));
	*(*ppBuf + iBuf++) = '\n';
	*(*ppBuf + iBuf) = '\0';

finalize_it:
ENDstrgen


BEGINmodExit
CODESTARTmodExit
	/* release what we no longer need */
	objRelease(errmsg, CORE_COMPONENT);
	objRelease(glbl, CORE_COMPONENT);
	objRelease(parser, CORE_COMPONENT);
	objRelease(datetime, CORE_COMPONENT);
ENDmodExit


BEGINqueryEtryPt
CODESTARTqueryEtryPt
CODEqueryEtryPt_STD_SMOD_QUERIES
ENDqueryEtryPt


BEGINmodInit(smtradfile)
CODESTARTmodInit
	*ipIFVersProvided = CURR_MOD_IF_VERSION; /* we only support the current interface specification */
CODEmodInit_QueryRegCFSLineHdlr
	CHKiRet(objUse(glbl, CORE_COMPONENT));
	CHKiRet(objUse(errmsg, CORE_COMPONENT));
	CHKiRet(objUse(parser, CORE_COMPONENT));
	CHKiRet(objUse(datetime, CORE_COMPONENT));

	dbgprintf("traditional file format strgen init called, compiled with version %s\n", VERSION);
	dbgprintf("GetStrgenName addr %p\n", GetStrgenName);
ENDmodInit
