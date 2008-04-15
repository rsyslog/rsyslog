/* The ctok_token object
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
#ifndef INCLUDED_CTOK_TOKEN_H
#define INCLUDED_CTOK_TOKEN_H

#include "obj.h"
#include "var.h"

/* the tokens... I use numbers below so that the tokens can be easier
 * identified in debug output. These ID's are also partly resused as opcodes.
 * As such, they should be kept below 1,000 so that they do not interfer
 * with the rest of the opcodes.
 */
typedef struct {
	BEGINobjInstance;	/* Data to implement generic object - MUST be the first data element! */
	enum {
		ctok_INVALID = 0,
		ctok_OR = 1,
		ctok_AND = 2,
		ctok_PLUS = 3,
		ctok_MINUS = 4,
		ctok_TIMES = 5,	 /* "*" */
		ctok_DIV = 6,
		ctok_MOD = 7,
		ctok_NOT = 8,
		ctok_RPAREN = 9,
		ctok_LPAREN = 10,
		ctok_COMMA = 11,
		ctok_SYSVAR = 12,
		ctok_MSGVAR = 13,
		ctok_SIMPSTR = 14,
		ctok_TPLSTR = 15,
		ctok_NUMBER = 16,
		ctok_FUNCTION = 17,
		ctok_THEN = 18,
		ctok_STRADD = 19,
		ctok_CMP_EQ = 100, /* all compare operations must be in a row */
		ctok_CMP_NEQ = 101,
		ctok_CMP_LT = 102,
		ctok_CMP_GT = 103,
		ctok_CMP_LTEQ = 104,
		ctok_CMP_CONTAINS = 105,
		ctok_CMP_STARTSWITH = 106,
		ctok_CMP_CONTAINSI = 107,
		ctok_CMP_STARTSWITHI = 108,
		ctok_CMP_GTEQ = 109, /* end compare operations */
	} tok;
	var_t *pVar;
	//cstr_t *pstrVal;
	//int64 intVal;
} ctok_token_t;


/* interfaces */
BEGINinterface(ctok_token) /* name must also be changed in ENDinterface macro! */
	INTERFACEObjDebugPrint(ctok_token);
	rsRetVal (*Construct)(ctok_token_t **ppThis);
	rsRetVal (*ConstructFinalize)(ctok_token_t __attribute__((unused)) *pThis);
	rsRetVal (*Destruct)(ctok_token_t **ppThis);
	rsRetVal (*UnlinkVar)(ctok_token_t *pThis, var_t **ppVar);
	int (*IsCmpOp)(ctok_token_t *pThis);
ENDinterface(ctok_token)
#define ctok_tokenCURR_IF_VERSION 1 /* increment whenever you change the interface structure! */


/* prototypes */
PROTOTYPEObj(ctok_token);

#endif /* #ifndef INCLUDED_CTOK_TOKEN_H */
