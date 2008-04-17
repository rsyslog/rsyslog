/* The sysvar object. So far, no instance can be defined (makes logically no
 * sense).
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
#ifndef INCLUDED_SYSVAR_H
#define INCLUDED_SYSVAR_H

/* the sysvar object - not really used... */
typedef struct sysvar_s {
	BEGINobjInstance;	/* Data to implement generic object - MUST be the first data element! */
} sysvar_t;


/* interfaces */
BEGINinterface(sysvar) /* name must also be changed in ENDinterface macro! */
	INTERFACEObjDebugPrint(sysvar);
	rsRetVal (*Construct)(sysvar_t **ppThis);
	rsRetVal (*ConstructFinalize)(sysvar_t __attribute__((unused)) *pThis);
	rsRetVal (*Destruct)(sysvar_t **ppThis);
	rsRetVal (*GetVar)(cstr_t *pstrPropName, var_t **ppVar);
ENDinterface(sysvar)
#define sysvarCURR_IF_VERSION 1 /* increment whenever you change the interface structure! */


/* prototypes */
PROTOTYPEObj(sysvar);

#endif /* #ifndef INCLUDED_SYSVAR_H */
