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
	VARTYPE_INT64 = 5,
	VARTYPE_CSTR = 6,
	VARTYPE_SYSLOGTIME = 7
} varType_t;

/* the var object */
typedef struct var_s {
	BEGINobjInstance;	/* Data to implement generic object - MUST be the first data element! */
	cstr_t *pcsName;
	varType_t varType;
	union {
		short vShort;
		int vInt;
		long vLong;
		int64 vInt64;
		cstr_t *vpCStr; /* used for both rsCStr and psz */
		syslogTime_t vSyslogTime;

	} val;
} var_t;


/* interfaces */
BEGINinterface(var) /* name must also be changed in ENDinterface macro! */
	INTERFACEObjDebugPrint(var);
	rsRetVal (*Construct)(var_t **ppThis);
	rsRetVal (*ConstructFinalize)(var_t __attribute__((unused)) *pThis);
	rsRetVal (*Destruct)(var_t **ppThis);
	rsRetVal (*SetInt64)(var_t *pThis, int64 iVal);
	rsRetVal (*SetString)(var_t *pThis, cstr_t *pCStr);
	rsRetVal (*ConvForOperation)(var_t *pThis, var_t *pOther);
ENDinterface(var)
#define varCURR_IF_VERSION 1 /* increment whenever you change the interface above! */


/* prototypes */
PROTOTYPEObj(var);

#endif /* #ifndef INCLUDED_VAR_H */
