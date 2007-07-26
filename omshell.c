/* omshell.c
 * This is the implementation of the build-in shell output module.
 *
 * NOTE: read comments in module-template.h to understand how this file
 *       works!
 *
 * shell support was initially written by bkalkbrenner 2005-09-20
 *
 * File begun on 2007-07-20 by RGerhards (extracted from syslogd.c)
 * This file is under development and has not yet arrived at being fully
 * self-contained and a real object. So far, it is mostly an excerpt
 * of the "old" message code without any modifications. However, it
 * helps to have things at the right place one we go to the meat of it.
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
#include <assert.h>
#include "rsyslog.h"
#include "syslogd.h"
#include "syslogd-types.h"
#include "srUtils.h"
#include "omshell.h"
#include "module-template.h"

/* internal structures
 */
typedef struct _instanceData {
} instanceData;


BEGINcreateInstance
CODESTARTcreateInstance
ENDcreateInstance


BEGINisCompatibleWithFeature
CODESTARTisCompatibleWithFeature
	if(eFeat == sFEATURERepeatedMsgReduction)
		iRet = RS_RET_OK;
ENDisCompatibleWithFeature


BEGINfreeInstance
CODESTARTfreeInstance
	/* TODO: free the instance pointer (currently a leak, will go away) */
ENDfreeInstance


BEGINdbgPrintInstInfo
CODESTARTdbgPrintInstInfo
	printf("%s", f->f_un.f_file.f_fname);
ENDdbgPrintInstInfo


BEGINdoAction
	uchar *psz;
CODESTARTdoAction
	/* TODO: using f->f_un.f_file.f_name is not clean from the point of
	 * modularization. We'll change that as we go ahead with modularization.
	 * rgerhards, 2007-07-20
	 */
	dprintf("\n");
	iovCreate(f);
	psz = (uchar*) iovAsString(f);
	if(execProg((uchar*) f->f_un.f_file.f_fname, 1, (uchar*) psz) == 0)
		logerrorSz("Executing program '%s' failed", f->f_un.f_file.f_fname);
ENDdoAction


BEGINparseSelectorAct
CODESTARTparseSelectorAct
	p = *pp;
	/* yes, the if below is redundant, but I need it now. Will go away as
	 * the code further changes.  -- rgerhards, 2007-07-25
	 */
	if(*p == '^') {
		if((iRet = createInstance(&pData)) != RS_RET_OK)
			return iRet;
	}


	switch (*p)
	{
	case '^': /* bkalkbrenner 2005-09-20: execute shell command */
		dprintf("exec\n");
		++p;
		if((iRet = cflineParseFileName(f, p, (uchar*) f->f_un.f_file.f_fname)) == RS_RET_OK)
			if (f->f_type == F_FILE) {
				f->f_type = F_SHELL;
			}
		break;
	default:
		iRet = RS_RET_CONFLINE_UNPROCESSED;
		break;
	}

ENDparseSelectorAct


BEGINonSelectReadyWrite
CODESTARTonSelectReadyWrite
ENDonSelectReadyWrite


BEGINgetWriteFDForSelect
CODESTARTgetWriteFDForSelect
ENDgetWriteFDForSelect


BEGINqueryEtryPt
CODESTARTqueryEtryPt
CODEqueryEtryPt_STD_OMOD_QUERIES
ENDqueryEtryPt


BEGINmodInit(Shell)
CODESTARTmodInit
	*ipIFVersProvided = 1; /* so far, we only support the initial definition */
ENDmodInit

/*
 * vi:set ai:
 */
