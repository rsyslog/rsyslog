/* dnscache.c
 * Implementation of a real DNS cache
 *
 * File begun on 2011-06-06 by RGerhards
 * The initial implementation is far from being optimal. The idea is to
 * first get somethting that'S functionally OK, and then evolve the algorithm.
 * In any case, even the initial implementaton is far faster than what we had
 * before. -- rgerhards, 2011-06-06
 *
 * Copyright 2011 by Rainer Gerhards and Adiscon GmbH.
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
#include "config.h"

#include "rsyslog.h"
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <netdb.h>
#include <unistd.h>

#include "syslogd-types.h"
#include "glbl.h"
#include "errmsg.h"
#include "obj.h"
#include "unicode-helper.h"
#include "net.h"

/* in this initial implementation, we use a simple, non-optimized at all
 * linear list.
 */
/* module data structures */
struct dnscache_entry_s {
	struct sockaddr_storage addr;
	uchar *pszHostFQDN;
	uchar *ip;
	struct dnscache_entry_s *next;
	unsigned nUsed;
};
typedef struct dnscache_entry_s dnscache_entry_t;
struct dnscache_s {
	pthread_rwlock_t rwlock;
	dnscache_entry_t *root;
	unsigned nEntries;
};
typedef struct dnscache_s dnscache_t;
#define MAX_CACHE_ENTRIES 1000


/* static data */
DEFobjStaticHelpers
DEFobjCurrIf(glbl)
DEFobjCurrIf(errmsg)
static dnscache_t dnsCache;


/* init function (must be called once) */
rsRetVal
dnscacheInit(void)
{
	DEFiRet;
	dnsCache.root = NULL;
	dnsCache.nEntries = 0;
	pthread_rwlock_init(&dnsCache.rwlock, NULL);
	CHKiRet(objGetObjInterface(&obj)); /* this provides the root pointer for all other queries */
	CHKiRet(objUse(glbl, CORE_COMPONENT));
	CHKiRet(objUse(errmsg, CORE_COMPONENT));
finalize_it:
	RETiRet;
}

/* deinit function (must be called once) */
rsRetVal
dnscacheDeinit(void)
{
	DEFiRet;
	//TODO: free cache elements dnsCache.root = NULL;
	pthread_rwlock_destroy(&dnsCache.rwlock);
	objRelease(glbl, CORE_COMPONENT);
	objRelease(errmsg, CORE_COMPONENT);
	RETiRet;
}


/* destruct a cache entry.
 * Precondition: entry must already be unlinked from list
 */
static inline void
entryDestruct(dnscache_entry_t *etry)
{
	free(etry->pszHostFQDN);
	free(etry->ip);
	free(etry);
}


static inline dnscache_entry_t*
findEntry(struct sockaddr_storage *addr)
{
	dnscache_entry_t *etry;
	for(etry = dnsCache.root ; etry != NULL ; etry = etry->next) {
		if(SALEN((struct sockaddr*)addr) == SALEN((struct sockaddr*) &etry->addr) 
		   && !memcmp(addr, &etry->addr, SALEN((struct sockaddr*) addr)))
			break; /* in this case, we found our entry */
	}
	if(etry != NULL)
		++etry->nUsed; /* this is *not* atomic, but we can live with an occasional loss! */
	return etry;
}


/* This is a cancel-safe getnameinfo() version, because we learned
 * (via drd/valgrind) that getnameinfo() seems to have some issues
 * when being cancelled, at least if the module was dlloaded.
 * rgerhards, 2008-09-30
 */
static inline int
mygetnameinfo(const struct sockaddr *sa, socklen_t salen,
                       char *host, size_t hostlen,
                       char *serv, size_t servlen, int flags)
{
	int iCancelStateSave;
	int i;

	pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &iCancelStateSave);
	i = getnameinfo(sa, salen, host, hostlen, serv, servlen, flags);
	pthread_setcancelstate(iCancelStateSave, NULL);
	return i;
}


/* resolve an address.
 *
 * Please see http://www.hmug.org/man/3/getnameinfo.php (under Caveats)
 * for some explanation of the code found below. We do by default not
 * discard message where we detected malicouos DNS PTR records. However,
 * there is a user-configurabel option that will tell us if
 * we should abort. For this, the return value tells the caller if the
 * message should be processed (1) or discarded (0).
 */
static rsRetVal
resolveAddr(struct sockaddr_storage *addr, uchar *pszHostFQDN, uchar *ip)
{
	DEFiRet;
	int error;
	sigset_t omask, nmask;
	struct addrinfo hints, *res;
	
	assert(addr != NULL);
	assert(pszHostFQDN != NULL);

        error = mygetnameinfo((struct sockaddr *)addr, SALEN((struct sockaddr *)addr),
			    (char*) ip, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
        if(error) {
                dbgprintf("Malformed from address %s\n", gai_strerror(error));
		ABORT_FINALIZE(RS_RET_INVALID_SOURCE);
	}

	if(!glbl.GetDisableDNS()) {
		sigemptyset(&nmask);
		sigaddset(&nmask, SIGHUP);
		pthread_sigmask(SIG_BLOCK, &nmask, &omask);

		error = mygetnameinfo((struct sockaddr *)addr, SALEN((struct sockaddr *) addr),
				    (char*)pszHostFQDN, NI_MAXHOST, NULL, 0, NI_NAMEREQD);
		
dbgprintf("dnscache: error %d after 2nd mygetnameinfo\n", error);
		if(error == 0) {
			memset (&hints, 0, sizeof (struct addrinfo));
			hints.ai_flags = AI_NUMERICHOST;

			/* we now do a lookup once again. This one should fail,
			 * because we should not have obtained a non-numeric address. If
			 * we got a numeric one, someone messed with DNS!
			 */
			if(getaddrinfo ((char*)pszHostFQDN, NULL, &hints, &res) == 0) {
				uchar szErrMsg[1024];
				freeaddrinfo (res);
				/* OK, we know we have evil. The question now is what to do about
				 * it. One the one hand, the message might probably be intended
				 * to harm us. On the other hand, losing the message may also harm us.
				 * Thus, the behaviour is controlled by the $DropMsgsWithMaliciousDnsPTRRecords
				 * option. If it tells us we should discard, we do so, else we proceed,
				 * but log an error message together with it.
				 * time being, we simply drop the name we obtained and use the IP - that one
				 * is OK in any way. We do also log the error message. rgerhards, 2007-07-16
		 		 */
		 		if(glbl.GetDropMalPTRMsgs() == 1) {
					snprintf((char*)szErrMsg, sizeof(szErrMsg) / sizeof(uchar),
						 "Malicious PTR record, message dropped "
						 "IP = \"%s\" HOST = \"%s\"",
						 ip, pszHostFQDN);
					errmsg.LogError(0, RS_RET_MALICIOUS_ENTITY, "%s", szErrMsg);
					pthread_sigmask(SIG_SETMASK, &omask, NULL);
					ABORT_FINALIZE(RS_RET_MALICIOUS_ENTITY);
				}

				/* Please note: we deal with a malicous entry. Thus, we have crafted
				 * the snprintf() below so that all text is in front of the entry - maybe
				 * it contains characters that make the message unreadable
				 * (OK, I admit this is more or less impossible, but I am paranoid...)
				 * rgerhards, 2007-07-16
				 */
				snprintf((char*)szErrMsg, sizeof(szErrMsg) / sizeof(uchar),
					 "Malicious PTR record (message accepted, but used IP "
					 "instead of PTR name: IP = \"%s\" HOST = \"%s\"",
					 ip, pszHostFQDN);
				errmsg.LogError(0, NO_ERRCODE, "%s", szErrMsg);

				error = 1; /* that will trigger using IP address below. */
			}
		}
		pthread_sigmask(SIG_SETMASK, &omask, NULL);
	}

dbgprintf("dnscache: error %d, DisableDNS %d\n", error, glbl.GetDisableDNS());
        if(error || glbl.GetDisableDNS()) {
                dbgprintf("Host name for your address (%s) unknown\n", ip);
		strcpy((char*) pszHostFQDN, (char*)ip);
        }

finalize_it:
	RETiRet;
}


/* evict an entry from the cache. We should try to evict one that does
 * not decrease the hit rate that much, but we do not try to hard currently
 * (as the base cache data structure may change).
 * This MUST NOT be called when the cache is empty!
 * rgerhards, 2011-06-06
 */
static inline void
evictEntry(void)
{
	dnscache_entry_t *prev, *evict, *prevEvict, *etry;
	unsigned lowest;

	prev = prevEvict = NULL;
	evict = dnsCache.root;
	lowest = evict->nUsed;
	for(etry = dnsCache.root->next ; etry != NULL ; etry = etry->next) {
		if(etry->nUsed < lowest) {
			evict = etry;
			lowest = etry->nUsed;
			prevEvict = prev;
		}
		prev = etry;
	}

	/* found lowest, unlink */
	if(prevEvict == NULL) { /* remove root? */
		dnsCache.root = evict->next;
	} else {
		prevEvict = evict->next;
	}
	entryDestruct(evict);
}


/* add a new entry to the cache. This means the address is resolved and
 * then added to the cache.
 */
static inline rsRetVal
addEntry(struct sockaddr_storage *addr, dnscache_entry_t **pEtry)
{
	uchar pszHostFQDN[NI_MAXHOST];
	uchar ip[80]; /* 80 is safe for larges IPv6 addr */
	dnscache_entry_t *etry;
	DEFiRet;
	CHKiRet(resolveAddr(addr, pszHostFQDN, ip));
	CHKmalloc(etry = MALLOC(sizeof(dnscache_entry_t)));
	CHKmalloc(etry->pszHostFQDN = ustrdup(pszHostFQDN));
	CHKmalloc(etry->ip = ustrdup(ip));
	memcpy(&etry->addr, addr, SALEN((struct sockaddr*) addr));
	etry->nUsed = 0;
	*pEtry = etry;

	/* add to list. Currently, we place the new element always at
	 * the root node. This needs to be optimized later. 2011-06-06
	 */
	pthread_rwlock_unlock(&dnsCache.rwlock); /* release read lock */
	pthread_rwlock_wrlock(&dnsCache.rwlock); /* and re-aquire for writing */
	if(dnsCache.nEntries >= MAX_CACHE_ENTRIES) {
		evictEntry();
	}
	etry->next = dnsCache.root;
	dnsCache.root = etry;
	pthread_rwlock_unlock(&dnsCache.rwlock);
	pthread_rwlock_rdlock(&dnsCache.rwlock); /* TODO: optimize this! */

finalize_it:
	RETiRet;
}


/* validate if an entry is still valid and, if not, re-query it.
 * In the initial implementation, this is a dummy!
 * TODO: implement!
 */
static inline rsRetVal
validateEntry(dnscache_entry_t __attribute__((unused)) *etry, struct sockaddr_storage __attribute__((unused)) *addr)
{
	return RS_RET_OK;
}


/* This is the main function: it looks up an entry and returns it's name
 * and IP address. If the entry is not yet inside the cache, it is added.
 * If the entry can not be resolved, an error is reported back.
 */
rsRetVal
dnscacheLookup(struct sockaddr_storage *addr, uchar *pszHostFQDN, uchar *ip)
{
	dnscache_entry_t *etry;
	DEFiRet;

	pthread_rwlock_rdlock(&dnsCache.rwlock); /* TODO: optimize this! */
	etry = findEntry(addr);
	dbgprintf("dnscache: entry %p found\n", etry);
	if(etry == NULL) {
		CHKiRet(addEntry(addr, &etry));
	} else {
		CHKiRet(validateEntry(etry, addr));
	}
	// TODO/QUESTION: can we get rid of the strcpy?
dbgprintf("XXXX: hostn '%s', ip '%s'\n", etry->pszHostFQDN, etry->ip);
	strcpy((char*)pszHostFQDN, (char*)etry->pszHostFQDN);
	strcpy((char*)ip, (char*)etry->ip);

finalize_it:
	pthread_rwlock_unlock(&dnsCache.rwlock);
dbgprintf("XXXX: dnscacheLookup finished, iRet=%d\n", iRet);
	if(iRet != RS_RET_OK && iRet != RS_RET_ADDRESS_UNKNOWN) {
		DBGPRINTF("dnscacheLookup failed with iRet %d\n", iRet);
		strcpy((char*) pszHostFQDN, "???");
		strcpy((char*) ip, "???");
	}
	RETiRet;
}
