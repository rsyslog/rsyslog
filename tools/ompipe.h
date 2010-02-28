/* ompipe.h
 * These are the definitions for the build-in pipe output module.
 *
 * Copyright 2007-2010 Rainer Gerhards and Adiscon GmbH.
 *
 * This pipe is part of rsyslog.
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
 * A copy of the GPL can be found in the pipe "COPYING" in this distribution.
 */
#ifndef	OMPIPE_H_INCLUDED
#define	OMPIPE_H_INCLUDED 1

/* prototypes */
rsRetVal modInitPipe(int iIFVersRequested __attribute__((unused)), int *ipIFVersProvided, rsRetVal (**pQueryEtryPt)(), rsRetVal (*pHostQueryEtryPt)(uchar*, rsRetVal (**)()), modInfo_t*);

#endif /* #ifndef OMPIPE_H_INCLUDED */
/* vi:set ai:
 */
