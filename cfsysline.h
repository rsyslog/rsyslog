/* Definition of the cfsysline (config file system line) object.
 *
 * Copyright 2007 Rainer Gerhards and Adiscon GmbH.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * A copy of the GPL can be found in the file "COPYING" in this distribution.
 */

#ifndef CFSYSLINE_H_INCLUDED
#define CFSYSLINE_H_INCLUDED

#include "linkedlist.h"

/* types of configuration handlers
 */
typedef enum cslCmdHdlrType {
	eCmdHdlrInvalid = 0,		/* invalid handler type - indicates a coding error */
	eCmdHdlrCustomHandler,		/* custom handler, just call handler function */
	eCmdHdlrUID,
	eCmdHdlrGID,
	eCmdHdlrBinary,
	eCmdHdlrFileCreateMode,
	eCmdHdlrInt,
	eCmdHdlrGetChar
} ecslCmdHdrlType;

/* this is a single entry for a parse routine. It describes exactly
 * one entry point/handler.
 * The short name is cslch (Configfile SysLine CommandHandler)
 */
struct cslCmdHdlr_s { /* config file sysline parse entry */
	ecslCmdHdrlType eType;			/* which type of handler is this? */
	rsRetVal (*cslCmdHdlr)();		/* function pointer to use with handler (params depending on eType) */
	void *pData;				/* user-supplied data pointer */
};
typedef struct cslCmdHdlr_s cslCmdHdlr_t;


/* this is the list of known configuration commands with pointers to
 * their handlers.
 * The short name is cslc (Configfile SysLine Command)
 */
struct cslCmd_s { /* config file sysline parse entry */
	int bChainingPermitted;			/* may multiple handlers be chained for this command? */
	linkedList_t llCmdHdlrs;	/* linked list of command handlers */
};
typedef struct cslCmd_s cslCmd_t;

/* prototypes */
rsRetVal regCfSysLineHdlr(uchar *pCmdName, int bChainingPermitted, ecslCmdHdrlType eType, rsRetVal (*pHdlr)(), void *pData);
rsRetVal unregCfSysLineHdlrs(void);
rsRetVal processCfSysLineCommand(uchar *pCmd, uchar **p);
rsRetVal cfsyslineInit(void);
void dbgPrintCfSysLineHandlers(void);

#endif /* #ifndef CFSYSLINE_H_INCLUDED */
