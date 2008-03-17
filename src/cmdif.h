/* This header defines the interface between the relp engine
 * and relp commands. A consistent interface is used to dispatch
 * commands to the respective command handlers. All handlers shall
 * use the macros provided in this file. This is especially important
 * as the interface may evolve in the future and the macros most
 * probably will hide the details.
 *
 * Please note that for each command there is a server- and a client
 * based handler. They, as well as all commands, are provided in separate
 * source files so that we can get to a nice granularity inside our
 * library (just in case if a resource-constrained caller needs to pull
 * just the bare essentials into its binary).
 *
 * Copyright 2008 by Rainer Gerhards and Adiscon GmbH.
 *
 * This file is part of librelp.
 *
 * Librelp is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Librelp is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with librelp.  If not, see <http://www.gnu.org/licenses/>.
 *
 * A copy of the GPL can be found in the file "COPYING" in this distribution.
 *
 * If the terms of the GPL are unsuitable for your needs, you may obtain
 * a commercial license from Adiscon. Contact sales@adiscon.com for further
 * details.
 *
 * ALL CONTRIBUTORS PLEASE NOTE that by sending contributions, you assign
 * your copyright to Adiscon GmbH, Germany. This is necessary to permit the
 * dual-licensing set forth here. Our apologies for this inconvenience, but
 * we sincerely believe that the dual-licensing model helps us provide great
 * free software while at the same time obtaining some develoment funding.
 */
#ifndef RELP_CMDIF_H_INCLUDED
#define	RELP_CMDIF_H_INCLUDED

#include "relpframe.h"

#define PROTOTYPEcommand(type, cmd) \
	relpRetVal relp##type##C##cmd(relpFrame_t *pFrame, relpSess_t *pSess);

#define BEGINcommand(type, cmd) \
	relpRetVal relp##type##C##cmd(relpFrame_t *pFrame, relpSess_t *pSess) \
	{ \
		RELPOBJ_assert(pFrame, Frame); \
		RELPOBJ_assert(pSess, Sess);

#define ENDcommand \
		LEAVE_RELPFUNC; \
	}


#endif /* #ifndef RELP_CMDIF_H_INCLUDED */
