/* The prop object.
 *
 * This implements props within rsyslog.
 *
 * Copyright 2009 Rainer Gerhards and Adiscon GmbH.
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
#ifndef INCLUDED_PROP_H
#define INCLUDED_PROP_H

/* the prop object */
struct prop_s {
	BEGINobjInstance;	/* Data to implement generic object - MUST be the first data element! */
};

/* interfaces */
BEGINinterface(prop) /* name must also be changed in ENDinterface macro! */
	INTERFACEObjDebugPrint(prop);
	rsRetVal (*Construct)(prop_t **ppThis);
	rsRetVal (*ConstructFinalize)(prop_t __attribute__((unused)) *pThis);
	rsRetVal (*Destruct)(prop_t **ppThis);
ENDinterface(prop)
#define propCURR_IF_VERSION 1 /* increment whenever you change the interface structure! */


/* prototypes */
PROTOTYPEObj(prop);

#endif /* #ifndef INCLUDED_PROP_H */
