/* The var object.
 *
 * Copyright 2008 Rainer Gerhards and Adiscon GmbH.
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
#ifndef INCLUDED_VAR_H
#define INCLUDED_VAR_H

#include "stringbuf.h"

/* data types */
typedef enum {
	VARTYPE_NONE = 0, /* currently no value set */
	VARTYPE_STR = 1,
	VARTYPE_NUMBER = 2,
	VARTYPE_SYSLOGTIME = 3
} varType_t;

/* the var object */
typedef struct var_s {
	BEGINobjInstance;	/* Data to implement generic object - MUST be the first data element! */
	cstr_t *pcsName;
	varType_t varType;
	union {
		number_t num;
		cstr_t *pStr;
		syslogTime_t vSyslogTime;

	} val;
} var_t;


/* interfaces */
BEGINinterface(var) /* name must also be changed in ENDinterface macro! */
	INTERFACEObjDebugPrint(var);
	rsRetVal (*Construct)(var_t **ppThis);
	rsRetVal (*ConstructFinalize)(var_t __attribute__((unused)) *pThis);
	rsRetVal (*Destruct)(var_t **ppThis);
	rsRetVal (*SetNumber)(var_t *pThis, number_t iVal);
	rsRetVal (*SetString)(var_t *pThis, cstr_t *pCStr);
	rsRetVal (*ConvForOperation)(var_t *pThis, var_t *pOther);
	rsRetVal (*ConvToNumber)(var_t *pThis);
	rsRetVal (*ConvToBool)(var_t *pThis);
	rsRetVal (*ConvToString)(var_t *pThis);
	rsRetVal (*Duplicate)(var_t *pThis, var_t **ppNew);
ENDinterface(var)
#define varCURR_IF_VERSION 1 /* increment whenever you change the interface above! */


/* prototypes */
PROTOTYPEObj(var);

#endif /* #ifndef INCLUDED_VAR_H */
