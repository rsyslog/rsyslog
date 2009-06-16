/* The zlibw object. It encapsulates the zlib functionality. The primary
 * purpose of this wrapper class is to enable rsyslogd core to be build without
 * zlib libraries.
 *
 * Copyright 2009 Rainer Gerhards and Adiscon GmbH.
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
#ifndef INCLUDED_ZLIBW_H
#define INCLUDED_ZLIBW_H

#include <zlib.h>

/* interfaces */
BEGINinterface(zlibw) /* name must also be changed in ENDinterface macro! */
	int (*DeflateInit)(z_streamp strm, int);
	int (*DeflateInit2)(z_streamp strm, int level, int method, int windowBits, int memLevel, int strategy);
	int (*Deflate)(z_streamp strm, int);
	int (*DeflateEnd)(z_streamp strm);
ENDinterface(zlibw)
#define zlibwCURR_IF_VERSION 1 /* increment whenever you change the interface structure! */


/* prototypes */
PROTOTYPEObj(zlibw);

/* the name of our library binary */
#define LM_ZLIBW_FILENAME "lmzlibw"

#endif /* #ifndef INCLUDED_ZLIBW_H */
