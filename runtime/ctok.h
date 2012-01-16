/* The ctok object (implements a config file tokenizer).
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
