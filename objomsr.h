/* Definition of the omsr (omodStringRequest) object.
 *
 * Copyright 2007 Rainer Gerhards and Adiscon GmbH.
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

#ifndef OBJOMSR_H_INCLUDED
#define OBJOMSR_H_INCLUDED

/* define flags for required template options */
#define OMSR_NO_RQD_TPL_OPTS	0
#define OMSR_RQD_TPL_OPT_SQL	1
/* next option is 2, 4, 8, ... */

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
int OMSRgetEntryCount(omodStringRequest_t *pThis);
int OMSRgetEntry(omodStringRequest_t *pThis, int iEntry, uchar **ppTplName, int *piTplOpts);

#endif /* #ifndef OBJOMSR_H_INCLUDED */
