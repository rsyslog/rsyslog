/* omfile.h
 * These are the definitions for the build-in file output module.
 *
 * File begun on 2007-07-21 by RGerhards (extracted from syslogd.c)
 *
 * Copyright 2007-2010 Rainer Gerhards and Adiscon GmbH.
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
#ifndef	OMFILE_H_INCLUDED
#define	OMFILE_H_INCLUDED 1

/* prototypes */
rsRetVal modInitFile(int iIFVersRequested __attribute__((unused)), int *ipIFVersProvided, rsRetVal (**pQueryEtryPt)(), rsRetVal (*pHostQueryEtryPt)(uchar*, rsRetVal (**)()), modInfo_t*);

/* the define below is dirty, but we need it for ompipe integration. There is no
 * other way to have the functionality (well, one way would be to go through the
 * globals, but that seems not yet justified. -- rgerhards, 2010-03-01
 */
uchar	*pszFileDfltTplName;
#endif /* #ifndef OMFILE_H_INCLUDED */
/* vi:set ai:
 */
