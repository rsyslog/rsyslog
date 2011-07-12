/* Definitions for config file handling (not yet an object).
 *
 * Copyright 2008 Rainer Gerhards and Adiscon GmbH.
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
#ifndef INCLUDED_CONF_H
#define INCLUDED_CONF_H
#include "action.h"

/* definitions used for doNameLine to differentiate between different command types
 * (with otherwise identical code). This is a left-over from the previous config
 * system. It stays, because it is still useful. So do not wonder why it looks
 * somewhat strange (at least its name). -- rgerhards, 2007-08-01
 */
enum eDirective { DIR_TEMPLATE = 0, DIR_OUTCHANNEL = 1, DIR_ALLOWEDSENDER = 2};
extern ecslConfObjType currConfObj;
extern int bConfStrictScoping;	/* force strict scoping during config processing? */

/* interfaces */
BEGINinterface(conf) /* name must also be changed in ENDinterface macro! */
	rsRetVal (*doNameLine)(uchar **pp, void* pVal);
	rsRetVal (*cfsysline)(uchar *p);
	rsRetVal (*doModLoad)(uchar **pp, __attribute__((unused)) void* pVal);
	rsRetVal (*GetNbrActActions)(rsconf_t *conf, int *);
	/* version 4 -- 2010-07-23 rgerhards */
	/* "just" added global variables
	 * FYI: we reconsider repacking as a non-object, as only the core currently
	 * accesses this module. The current object structure complicates things without
	 * any real benefit.
	 */
	/* version 5 -- 2011-04-19 rgerhards */
	/* complete revamp, we now use the rsconf object */
	/* version 6 -- 2011-07-06 rgerhards */
	/* again a complete revamp, using flex/bison based parser now */
ENDinterface(conf)
#define confCURR_IF_VERSION 6 /* increment whenever you change the interface structure! */
/* in Version 3, entry point "ReInitConf()" was removed, as we do not longer need
 * to support restart-type HUP -- rgerhards, 2009-07-15
 */


/* prototypes */
PROTOTYPEObj(conf);


/* TODO: the following 2 need to go in conf obj interface... */
rsRetVal cflineParseTemplateName(uchar** pp, omodStringRequest_t *pOMSR, int iEntry, int iTplOpts, uchar *dfltTplName);
rsRetVal cflineParseFileName(uchar* p, uchar *pFileName, omodStringRequest_t *pOMSR, int iEntry, int iTplOpts, uchar *pszTpl);

/* more dirt to cover the new config interface (will go away...) */
rsRetVal cflineProcessTagSelector(uchar **pline);
rsRetVal cflineProcessHostSelector(uchar **pline);
rsRetVal cflineProcessTradPRIFilter(uchar **pline, rule_t *pRule);
rsRetVal cflineProcessPropFilter(uchar **pline, rule_t *f);
rsRetVal cflineDoAction(rsconf_t *conf, uchar **p, action_t **ppAction);
extern EHostnameCmpMode eDfltHostnameCmpMode;
extern cstr_t *pDfltHostnameCmp;
extern cstr_t *pDfltProgNameCmp;

#endif /* #ifndef INCLUDED_CONF_H */
