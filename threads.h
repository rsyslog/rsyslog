/* Definition of the threading support module.
 *
 * Copyright 2007 Rainer Gerhards and Adiscon GmbH.
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

#ifndef THREADS_H_INCLUDED
#define THREADS_H_INCLUDED

/* this is the first approach to a queue, this time with static
 * memory.
 */
typedef struct {
	void** pbuf;
	long head, tail;
	int full, empty;
	pthread_mutex_t *mut;
	pthread_cond_t *notFull, *notEmpty;
} msgQueue;

/* prototypes */
msgQueue *queueInit (void);
void queueDelete (msgQueue *q);
void queueAdd (msgQueue *q, void* in);
void queueDel (msgQueue *q, void **out);

/* go-away's */
extern int iMainMsgQueueSize;
extern msgQueue *pMsgQueue;

#endif /* #ifndef THREADS_H_INCLUDED */
