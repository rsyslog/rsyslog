/* The ctok object (implements a config file tokenizer).
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
#ifndef INCLUDED_CTOK_H
#define INCLUDED_CTOK_H

#include "obj.h"
#include "stringbuf.h"

/* the tokens... I use numbers below so that the tokens can be easier
 * identified in debug output. */
typedef struct {
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
		ctok_CMP_EQ = 16,
		ctok_CMP_NEQ = 17,
		ctok_CMP_LT = 18,
		ctok_CMP_GT = 19,
		ctok_CMP_LTEQ = 20,
		ctok_CMP_GTEQ = 21,
		ctok_NUMBER = 22
	} tok;
	rsCStrObj *pstrVal;
	int64 intVal;
} ctok_token_t;

/* the ctokession object */
typedef struct ctok_s {
	BEGINobjInstance;	/* Data to implement generic object - MUST be the first data element! */
	uchar *pp;		/* this points to the next unread character, it is a reminescent of pp in
				   the config parser code ;) */
} ctok_t;


/* prototypes */
rsRetVal ctokConstruct(ctok_t **ppThis);
rsRetVal ctokConstructFinalize(ctok_t __attribute__((unused)) *pThis);
rsRetVal ctokDestruct(ctok_t **ppThis);
rsRetVal ctokGetpp(ctok_t *pThis, uchar **pp);
rsRetVal ctokGetNextToken(ctok_t *pThis, ctok_token_t *pToken);
PROTOTYPEObjClassInit(ctok);
PROTOTYPEpropSetMeth(ctok, pp, uchar*);

#endif /* #ifndef INCLUDED_CTOK_H */
