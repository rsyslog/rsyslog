/* This header supplies atomic operations. So far, we rely on GCC's
 * atomic builtins. I have no idea if we can check them via autotools,
 * but I am making the necessary provisioning to live without them if
 * they are not available. Please note that you should only use the macros
 * here if you think you can actually live WITHOUT an explicit atomic operation,
 * because in the non-presence of them, we simply do it without atomicitiy.
 * Which, for word-aligned data types, usually (but only usually!) should work.
 *
 * We are using the functions described in
 * http:/gcc.gnu.org/onlinedocs/gcc/Atomic-Builtins.html
 *
 * THESE MACROS MUST ONLY BE USED WITH WORD-SIZED DATA TYPES!
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
#include "config.h" /* autotools! */

#ifndef INCLUDED_ATOMIC_H
#define INCLUDED_ATOMIC_H

/* set the following to 1 if we have atomic operations (and #undef it otherwise) */
/* #define DO_HAVE_ATOMICS 1 */
/* for this release, we disable atomic calls because there seem to be some
 * portability problems and we can not fix that without destabilizing the build.
 * They simply came in too late. -- rgerhards, 2008-04-02
 */
/* make sure they are not used!
#define ATOMIC_INC(data) ((void) __sync_fetch_and_add(&data, 1))
#define ATOMIC_DEC_AND_FETCH(data) __sync_sub_and_fetch(&data, 1)
*/
#define ATOMIC_INC(data) (++(data))

#endif /* #ifndef INCLUDED_ATOMIC_H */
