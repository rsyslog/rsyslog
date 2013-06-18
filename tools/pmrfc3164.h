/* pmrfc3164.h
 * These are the definitions for the RFC3164 parser module.
 *
 * File begun on 2009-11-04 by RGerhards
 *
 * Copyright 2009 Rainer Gerhards and Adiscon GmbH.
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
#ifndef	PMRFC3164_H_INCLUDED
#define	PMRFC3164_H_INCLUDED 1

/* prototypes */
rsRetVal modInitpmrfc3164(int iIFVersRequested __attribute__((unused)), int *ipIFVersProvided, rsRetVal (**pQueryEtryPt)(), rsRetVal (*pHostQueryEtryPt)(uchar*, rsRetVal (**)()), modInfo_t*);

#endif /* #ifndef PMRFC3164_H_INCLUDED */
/* vi:set ai:
 */
