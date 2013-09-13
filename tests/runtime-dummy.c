/* Testbench for rsyslog 
 * 
 * This are dummy calls for "runtime" routines which are not yet properly
 * abstracted and part of the actual runtime libraries. This module tries
 * to make the linker happy. Please note that it does NOT provide anything
 * more but the symbols. If a test requires these functions (or functions
 * that depend on them), this dummy can not be used.
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
#include "rsyslog.h"

int bReduceRepeatMsgs = 0;
int bActExecWhenPrevSusp = 0;
int iActExecOnceInterval = 1;
int MarkInterval = 30;
void *pMsgQueue = NULL;

void cflineClassic(void) {};
void selectorAddList(void) {};
void selectorConstruct(void) {};
void selectorDestruct(void) {};
rsRetVal createMainQueue(void) { return RS_RET_ERR; }

ruleset_t *pCurrRuleset;
/* these are required by some dynamically loaded modules */
