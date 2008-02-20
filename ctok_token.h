/* The ctok_token object
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
#ifndef INCLUDED_CTOK_TOKEN_H
#define INCLUDED_CTOK_TOKEN_H

#include "obj.h"
#include "stringbuf.h"

/* the tokens... I use numbers below so that the tokens can be easier
 * identified in debug output. */
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
		ctok_CMP_EQ = 100, /* all compare operations must be in a row */
		ctok_CMP_NEQ = 101,
		ctok_CMP_LT = 102,
		ctok_CMP_GT = 103,
		ctok_CMP_LTEQ = 104,
		ctok_CMP_GTEQ = 105, /* end compare operations */
	} tok;
	rsCStrObj *pstrVal;
	int64 intVal;
} ctok_token_t;

/* defines to handle compare operation tokens in a single if... */
#define ctok_tokenIsCmpOp(x) ((x)->tok >= ctok_CMP_EQ && (x)->tok <= ctok_CMP_GTEQ)


/* prototypes */
rsRetVal ctok_tokenConstruct(ctok_token_t **ppThis);
rsRetVal ctok_tokenConstructFinalize(ctok_token_t __attribute__((unused)) *pThis);
rsRetVal ctok_tokenDestruct(ctok_token_t **ppThis);
PROTOTYPEObjClassInit(ctok_token);

#endif /* #ifndef INCLUDED_CTOK_TOKEN_H */
