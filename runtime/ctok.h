/* The ctok object (implements a config file tokenizer).
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
#ifndef INCLUDED_CTOK_H
#define INCLUDED_CTOK_H

#include "obj.h"
#include "stringbuf.h"
#include "ctok_token.h"

/* the ctokession object */
typedef struct ctok_s {
	BEGINobjInstance;	/* Data to implement generic object - MUST be the first data element! */
	uchar *pp;		/* this points to the next unread character, it is a reminescent of pp in
				   the config parser code ;) */
	ctok_token_t *pUngotToken; /* buffer for ctokUngetToken(), NULL if not set */
} ctok_t;


/* interfaces */
BEGINinterface(ctok) /* name must also be changed in ENDinterface macro! */
	INTERFACEObjDebugPrint(ctok);
	INTERFACEpropSetMeth(ctok, pp, uchar*);
	rsRetVal (*Construct)(ctok_t **ppThis);
	rsRetVal (*ConstructFinalize)(ctok_t __attribute__((unused)) *pThis);
	rsRetVal (*Destruct)(ctok_t **ppThis);
	rsRetVal (*Getpp)(ctok_t *pThis, uchar **pp);
	rsRetVal (*GetToken)(ctok_t *pThis, ctok_token_t **ppToken);
	rsRetVal (*UngetToken)(ctok_t *pThis, ctok_token_t *pToken);
ENDinterface(ctok)
#define ctokCURR_IF_VERSION 1 /* increment whenever you change the interface structure! */


/* prototypes */
PROTOTYPEObj(ctok);

#endif /* #ifndef INCLUDED_CTOK_H */
