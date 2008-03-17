/* Some support macros for doubly linked lists.
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
 * free software while at the same time obtaining some funding for further
 * development.
 */
#ifndef DBLLINKLIST_H_INCLUDED
#define	DBLLINKLIST_H_INCLUDED

/* DLL means "doubly linked list". A class utilizing this macros
 * must define a pNext and pPrev pointer inside the list elements and
 * it must also define a root and last pointer. Please note that the
 * macros are NOT thread-safe. If you intend to use them in a threaed
 * environment, you must use proper synchronization.
 * NOTE: the caller must free the unlinked object, this is NOT done
 * in the macro!
 */
#define DLL_Del(pThis, pRoot, pLast) \
	if(pThis->pPrev != NULL) \
		pThis->pPrev->pNext = pThis->pNext; \
	if(pThis->pNext != NULL) \
		pThis->pNext->pPrev = pThis->pPrev; \
	if(pThis == pRoot) \
		pRoot = pThis->pNext; \
	if(pThis == pLast) \
		pLast = pThis->pPrev;

/* elements are always added to the tail of the list */
#define DLL_Add(pThis, pRoot, pLast) \
		if(pRoot == NULL) { \
			pRoot = pThis; \
			pLast = pThis; \
		} else { \
			pThis->pPrev = pLast; \
			pLast->pNext = pThis; \
			pLast = pThis; \
		}

#endif /* #ifndef DBLLINKLIST_H_INCLUDED */
