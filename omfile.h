/* omfile.h
 * These are the definitions for the build-in file output module.
 *
 * File begun on 2007-07-21 by RGerhards (extracted from syslogd.c)
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
#ifndef	OMFILE_H_INCLUDED
#define	OMFILE_H_INCLUDED 1

/* prototypes */
rsRetVal modInitFile(int iIFVersRequested __attribute__((unused)), int *ipIFVersProvided, rsRetVal (**pQueryEtryPt)(), rsRetVal (*pHostQueryEtryPt)(uchar*, rsRetVal (**)()));
rsRetVal setDynaFileCacheSize(void __attribute__((unused)) *pVal, int iNewVal); // TODO: remove me after testing!

#endif /* #ifndef OMFILE_H_INCLUDED */
/*
 * vi:set ai:
 */
