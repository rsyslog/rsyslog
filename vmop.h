/* The vmop object.
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
#ifndef INCLUDED_VMOP_H
#define INCLUDED_VMOP_H

/* machine instructions types */
typedef enum {	 /* do NOT start at 0 to detect uninitialized types after calloc() */
	opcode_INVALID = 0,
	opcode_PUSH = 1
} opcode_t;

/* the vmop object */
typedef struct vmop_s {
	BEGINobjInstance;	/* Data to implement generic object - MUST be the first data element! */
	opcode_t opcode;
	union {
		var_t *pVar;
		/* TODO: add function pointer */
	} operand;
} vmop_t;


/* prototypes */
rsRetVal vmopConstruct(vmop_t **ppThis);
rsRetVal vmopConstructFinalize(vmop_t __attribute__((unused)) *pThis);
rsRetVal vmopDestruct(vmop_t **ppThis);
rsRetVal vmopSetOpcode(vmop_t *pThis, opcode_t opcode);
rsRetVal vmopSetVar(vmop_t *pThis, var_t *pVar);
PROTOTYPEObjClassInit(vmop);

#endif /* #ifndef INCLUDED_VMOP_H */
