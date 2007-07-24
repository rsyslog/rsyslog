/* omdiscard.c
 * This is the implementation of the built-in discard output module.
 *
 * File begun on 2007-07-24 by RGerhards
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
#include "omdiscard.h"


/* query feature compatibility
 */
static rsRetVal isCompatibleWithFeature(syslogFeature __attribute__((unused)) eFeat)
{
	/* this module is incompatible with all currently-known optional
	 * syslog features. Turn them on if that changes over time.
	 */
	return RS_RET_INCOMPATIBLE;
}


/* call the shell action
 */
static rsRetVal doAction(__attribute__((unused)) selector_t *f)
{
	dprintf("Discarding message based on selector config\n");
	return RS_RET_DISCARDMSG;
}


/* free an instance
 */
static rsRetVal freeInstance(selector_t *f)
{
	assert(f != NULL);
	return RS_RET_OK;
}


/* try to process a selector action line. Checks if the action
 * applies to this module and, if so, processed it. If not, it
 * is left untouched. The driver will then call another module
 */
static rsRetVal parseSelectorAct(uchar **pp, selector_t *f)
{
	uchar *p;
	rsRetVal iRet = RS_RET_CONFLINE_PROCESSED;

	assert(pp != NULL);
	assert(f != NULL);

	p = *pp;

	if(*p == '~') {
		/* TODO: check the rest of the selector line - error reporting */
		dprintf("discard\n");
		f->f_type = F_DISCARD;
	} else {
		iRet = RS_RET_CONFLINE_UNPROCESSED;
	}

	if(iRet == RS_RET_CONFLINE_PROCESSED)
		*pp = p;
	return iRet;
}

/* query an entry point
 */
static rsRetVal queryEtryPt(uchar *name, rsRetVal (**pEtryPoint)())
{
	if((name == NULL) || (pEtryPoint == NULL))
		return RS_RET_PARAM_ERROR;

	*pEtryPoint = NULL;
	if(!strcmp((char*) name, "doAction")) {
		*pEtryPoint = doAction;
	} else if(!strcmp((char*) name, "parseSelectorAct")) {
		*pEtryPoint = parseSelectorAct;
	} else if(!strcmp((char*) name, "isCompatibleWithFeature")) {
		*pEtryPoint = isCompatibleWithFeature;
	} else if(!strcmp((char*) name, "freeInstance")) {
		*pEtryPoint = freeInstance; 
	}

	return(*pEtryPoint == NULL) ? RS_RET_NOT_FOUND : RS_RET_OK;
}

/* initialize the module
 *
 * Later, much more must be done. So far, we only return a pointer
 * to the queryEtryPt() function
 * TODO: do interface version checking & handshaking
 * iIfVersRequeted is the version of the interface specification that the
 * caller would like to see being used. ipIFVersProvided is what we
 * decide to provide.
 */
rsRetVal modInitDiscard(int iIFVersRequested __attribute__((unused)), int *ipIFVersProvided, rsRetVal (**pQueryEtryPt)())
{
	if((pQueryEtryPt == NULL) || (ipIFVersProvided == NULL))
		return RS_RET_PARAM_ERROR;

	*ipIFVersProvided = 1; /* so far, we only support the initial definition */

	*pQueryEtryPt = queryEtryPt;
	return RS_RET_OK;
}
/*
 * vi:set ai:
 */
