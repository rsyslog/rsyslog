/* The var object.
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
#ifndef INCLUDED_VAR_H
#define INCLUDED_VAR_H

#include "stringbuf.h"

/* data types */
typedef enum {
	VARTYPE_NONE = 0, /* currently no value set */
	VARTYPE_PSZ = 1,
	VARTYPE_SHORT = 2,
	VARTYPE_INT = 3,
	VARTYPE_LONG = 4,
	VARTYPE_CSTR = 5,
	VARTYPE_SYSLOGTIME = 6
} varType_t;

/* the var object */
typedef struct var_s {
	BEGINobjInstance;	/* Data to implement generic object - MUST be the first data element! */
	rsCStrObj *pcsName;
	varType_t varType;
	union {
		short vShort;
		int vInt;
		long vLong;
		rsCStrObj *vpCStr; /* used for both rsCStr and psz */
		syslogTime_t vSyslogTime;

	} val;
} var_t;


/* prototypes */
rsRetVal varConstruct(var_t **ppThis);
rsRetVal varConstructFinalize(var_t __attribute__((unused)) *pThis);
rsRetVal varDestruct(var_t **ppThis);
rsRetVal varSetString(var_t *pThis, rsCStrObj *pCStr);
PROTOTYPEObjClassInit(var);
PROTOTYPEObjDebugPrint(var);

#endif /* #ifndef INCLUDED_VAR_H */
