/* header for ratelimit.c
 *
 * Copyright 2012 Adiscon GmbH.
 *
 * This file is part of the rsyslog runtime library.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *       http://www.apache.org/licenses/LICENSE-2.0
 *       -or-
 *       see COPYING.ASL20 in the source distribution
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef INCLUDED_RATELIMIT_H
#define INCLUDED_RATELIMIT_H

struct ratelimit_s {
	unsigned nsupp;		/**< nbr of msgs suppressed */
	msg_t *pMsg;
	msg_t *repMsg;		/**< repeat message, temporary buffer */
	/* dummy field list - TODO: implement */
};

/* prototypes */
rsRetVal ratelimitNew(ratelimit_t **ppThis);
rsRetVal ratelimitMsg(msg_t *ppMsg, ratelimit_t *ratelimit);
msg_t * ratelimitGetRepeatMsg(ratelimit_t *ratelimit);
void ratelimitDestruct(ratelimit_t *pThis);
rsRetVal ratelimitModInit(void);
void ratelimitModExit(void);

#endif /* #ifndef INCLUDED_RATELIMIT_H */
