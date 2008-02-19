/* cfgtok.c - helper class to tokenize an input stream - which surprisingly
 * currently does not work with streams but with string. But that will
 * probably change over time ;) This class was originally written to support
 * the expression module but may evolve when (if) the expression module is
 * expanded (or aggregated) by a full-fledged ctoken based config parser.
 * Obviously, this class is used together with config files and not any other
 * parse function.
 *
 * Module begun 2008-02-19 by Rainer Gerhards
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

#include "config.h"
#include <stdlib.h>
#include <assert.h>

#include "rsyslog.h"
#include "template.h"
#include "ctok.h"

/* static data */
DEFobjStaticHelpers


/* Standard-Constructor
 */
BEGINobjConstruct(ctok) /* be sure to specify the object type also in END macro! */
ENDobjConstruct(ctok)


/* ConstructionFinalizer
 * rgerhards, 2008-01-09
 */
rsRetVal ctokConstructFinalize(ctok_t *pThis)
{
	DEFiRet;
	RETiRet;
}


/* destructor for the ctok object */
BEGINobjDestruct(ctok) /* be sure to specify the object type also in END and CODESTART macros! */
CODESTARTobjDestruct(ctok)
	/* ... then free resources */
ENDobjDestruct(ctok)

/* property set methods */
/* simple ones first */
DEFpropSetMeth(ctok, pp, uchar*)

/* return the current position of pp - most important as currently we do only
 * partial parsing, so the rest must know where to start from...
 * rgerhards, 2008-02-19
 */
rsRetVal
ctokGetpp(ctok_t *pThis, uchar **pp)
{
	DEFiRet;
	ASSERT(pp != NULL);
	*pp = pThis->pp;
	RETiRet;
}

BEGINObjClassInit(ctok, 1) /* class, version */
	OBJSetMethodHandler(objMethod_CONSTRUCTION_FINALIZER, ctokConstructFinalize);
ENDObjClassInit(ctok)

/* vi:set ai:
 */
