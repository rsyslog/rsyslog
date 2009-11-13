/* Definition of the omsr (omodStringRequest) object.
 *
 * Copyright 2007, 2009 Rainer Gerhards and Adiscon GmbH.
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

#ifndef OBJOMSR_H_INCLUDED
#define OBJOMSR_H_INCLUDED

/* define flags for required template options */
#define OMSR_NO_RQD_TPL_OPTS	0
#define OMSR_RQD_TPL_OPT_SQL	1
/* only one of OMSR_TPL_AS_ARRAY or _AS_MSG must be specified, if both are given
 * results are unpredictable.
 */
#define OMSR_TPL_AS_ARRAY	2	 /* introduced in 4.1.6, 2009-04-03 */
#define OMSR_TPL_AS_MSG		4	 /* introduced in 5.3.4, 2009-11-02 */
/* next option is 8, 16, 32, ... */

struct omodStringRequest_s {	/* strings requested by output module for doAction() */
	int iNumEntries;	/* number of array entries for data elements below */
	uchar **ppTplName;	/* pointer to array of template names */
	int *piTplOpts;/* pointer to array of check-options when pulling template */
};
typedef struct omodStringRequest_s omodStringRequest_t;

/* prototypes */
rsRetVal OMSRdestruct(omodStringRequest_t *pThis);
rsRetVal OMSRconstruct(omodStringRequest_t **ppThis, int iNumEntries);
rsRetVal OMSRsetEntry(omodStringRequest_t *pThis, int iEntry, uchar *pTplName, int iTplOpts);
rsRetVal OMSRgetSupportedTplOpts(unsigned long *pOpts);
int OMSRgetEntryCount(omodStringRequest_t *pThis);
int OMSRgetEntry(omodStringRequest_t *pThis, int iEntry, uchar **ppTplName, int *piTplOpts);

#endif /* #ifndef OBJOMSR_H_INCLUDED */
