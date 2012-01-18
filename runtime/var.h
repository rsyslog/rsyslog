/* The var object.
 *
 * Copyright 2008-2012 Adiscon GmbH.
 *
 * This file is part of the rsyslog runtime library.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *       http://www.apache.org/licenses/LICENSE-2.0
 *       -or-
 *       see COPYING.ASL20 in the source distribution
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
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
		es_str_t *str;
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
	rsRetVal (*Obj2Str)(var_t *pThis, cstr_t*);
	rsRetVal (*Duplicate)(var_t *pThis, var_t **ppNew);
ENDinterface(var)
#define varCURR_IF_VERSION 1 /* increment whenever you change the interface above! */


/* prototypes */
PROTOTYPEObj(var);

#endif /* #ifndef INCLUDED_VAR_H */
