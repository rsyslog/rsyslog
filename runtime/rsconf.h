/* The rsconf object. It models a complete rsyslog configuration.
 *
 * Copyright 2011 Rainer Gerhards and Adiscon GmbH.
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
#ifndef INCLUDED_RSCONF_H
#define INCLUDED_RSCONF_H

/* --- configuration objects (the plan is to have ALL upper layers in this file) --- */

/* the following structure is a container for all known templates
 * inside a specific configuration. -- rgerhards 2011-04-19
 */
struct templates_s {
	struct template *root;	/* the root of the template list */
	struct template *last;	/* points to the last element of the template list */
	struct template *lastStatic; /* last static element of the template list */
};


struct actions_s {
	unsigned nbrActions;		/* number of actions */
};

/* --- end configuration objects --- */

/* the rsconf object */
struct rsconf_s {
	BEGINobjInstance;	/* Data to implement generic object - MUST be the first data element! */
	templates_t templates;
	actions_t actions;
};


/* interfaces */
BEGINinterface(rsconf) /* name must also be changed in ENDinterface macro! */
	INTERFACEObjDebugPrint(rsconf);
	rsRetVal (*Construct)(rsconf_t **ppThis);
	rsRetVal (*ConstructFinalize)(rsconf_t __attribute__((unused)) *pThis);
	rsRetVal (*Destruct)(rsconf_t **ppThis);
ENDinterface(rsconf)
#define rsconfCURR_IF_VERSION 1 /* increment whenever you change the interface above! */


/* prototypes */
PROTOTYPEObj(rsconf);

#endif /* #ifndef INCLUDED_RSCONF_H */
