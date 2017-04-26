/* dnscache.c
 * Implementation of a real DNS cache
 *
 * File begun on 2011-06-06 by RGerhards
 * The initial implementation is far from being optimal. The idea is to
 * first get somethting that'S functionally OK, and then evolve the algorithm.
 * In any case, even the initial implementaton is far faster than what we had
 * before. -- rgerhards, 2011-06-06
 *
 * Copyright 2011-2016 by Rainer Gerhards and Adiscon GmbH.
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
#include <ctype.h>

#include "syslogd-types.h"
#include "glbl.h"
#include "errmsg.h"
#include "obj.h"
#include "unicode-helper.h"
#include "net.h"
#include "hashtable.h"
#include "prop.h"
#include "dnscache.h"

/* module data structures */
struct dnscache_entry_s {
	struct sockaddr_storage addr;
	prop_t *fqdn;
	prop_t *fqdnLowerCase;
	prop_t *localName; /* only local name, without domain part (if configured so) */
	prop_t *ip;
	struct dnscache_entry_s *next;
	unsigned nUsed;
};
typedef struct dnscache_entry_s dnscache_entry_t;
struct dnscache_s {
	pthread_rwlock_t rwlock;
	struct hashtable *ht;
	unsigned nEntries;
};
typedef struct dnscache_s dnscache_t;


/* static data */
DEFobjStaticHelpers;
DEFobjCurrIf(glbl);
DEFobjCurrIf(errmsg);
DEFobjCurrIf(prop) static dnscache_t dnsCache;
static prop_t *staticErrValue;


/* Our hash function.
 * TODO: check how well it performs on socket addresses!
 */
static unsigned int
hash_from_key_fn(void *k)
{
	int len;
	uchar *rkey = (uchar *)k; /* we treat this as opaque bytes */
	unsigned hashval = 1;

	len = SALEN((struct sockaddr *)k);
	while (len--)
		hashval = hashval * 33 + *rkey++;

	return hashval;
}

static int
key_equals_fn(void *key1, void *key2)
{
	return (SALEN((struct sockaddr *)key1) == SALEN((struct sockaddr *)key2) && !memcmp(key1, key2, SALEN((struct sockaddr *)key1)));
}

/* destruct a cache entry.
 * Precondition: entry must already be unlinked from list
 */
static void
entryDestruct(dnscache_entry_t *etry)
{
	if (etry->fqdn != NULL)
		prop.Destruct(&etry->fqdn);
	if (etry->fqdnLowerCase != NULL)
		prop.Destruct(&etry->fqdnLowerCase);
	if (etry->localName != NULL)
		prop.Destruct(&etry->localName);
	if (etry->ip != NULL)
		prop.Destruct(&etry->ip);
	free(etry);
}

/* init function (must be called once) */
rsRetVal
dnscacheInit(void)
{
	DEFiRet;
	if ((dnsCache.ht = create_hashtable(100, hash_from_key_fn, key_equals_fn,
		 (void (*)(void *))entryDestruct)) == NULL) {
		DBGPRINTF("dnscache: error creating hash table!\n");
		ABORT_FINALIZE(RS_RET_ERR); // TODO: make this degrade, but run!
	}
	dnsCache.nEntries = 0;
	pthread_rwlock_init(&dnsCache.rwlock, NULL);
	CHKiRet(objGetObjInterface(&obj)); /* this provides the root pointer for all other queries */
	CHKiRet(objUse(glbl, CORE_COMPONENT));
	CHKiRet(objUse(errmsg, CORE_COMPONENT));
	CHKiRet(objUse(prop, CORE_COMPONENT));

	prop.Construct(&staticErrValue);
	prop.SetString(staticErrValue, (uchar *)"???", 3);
	prop.ConstructFinalize(staticErrValue);
finalize_it:
	RETiRet;
}

/* deinit function (must be called once) */
rsRetVal
dnscacheDeinit(void)
{
	DEFiRet;
	prop.Destruct(&staticErrValue);
	hashtable_destroy(dnsCache.ht, 1); /* 1 => free all values automatically */
	pthread_rwlock_destroy(&dnsCache.rwlock);
	objRelease(glbl, CORE_COMPONENT);
	objRelease(errmsg, CORE_COMPONENT);
	objRelease(prop, CORE_COMPONENT);
	RETiRet;
}


static inline dnscache_entry_t *
findEntry(struct sockaddr_storage *addr)
{
	return ((dnscache_entry_t *)hashtable_search(dnsCache.ht, addr));
}


/* This is a cancel-safe getnameinfo() version, because we learned
 * (via drd/valgrind) that getnameinfo() seems to have some issues
 * when being cancelled, at least if the module was dlloaded.
 * rgerhards, 2008-09-30
 */
static int
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


/* get only the local part of the hostname and set it in cache entry */
static void
setLocalHostName(dnscache_entry_t *etry)
{
	uchar *fqdnLower;
	uchar *p;
	int count;
	int i;
	uchar hostbuf[NI_MAXHOST];

	if (glbl.GetPreserveFQDN()) {
		prop.AddRef(etry->fqdnLowerCase);
		etry->localName = etry->fqdnLowerCase;
		goto done;
	}

	/* strip domain, if configured for this entry */
	fqdnLower = propGetSzStr(etry->fqdnLowerCase);
	p = (uchar *)strchr((char *)fqdnLower, '.'); /* find start of domain name "machine.example.com" */
	if (p == NULL) {			     /* do we have a domain part? */
		prop.AddRef(etry->fqdnLowerCase);    /* no! */
		etry->localName = etry->fqdnLowerCase;
		goto done;
	}

	i = p - fqdnLower; /* length of hostname */
	memcpy(hostbuf, fqdnLower, i);
	hostbuf[i] = '\0';
	/* now check if we belong to any of the domain names that were specified
	 * in the -s command line option. If so, remove and we are done.
	 */
	if (glbl.GetStripDomains() != NULL) {
		count = 0;
		while (glbl.GetStripDomains()[count]) {
			if (strcmp((char *)(p + 1), glbl.GetStripDomains()[count]) == 0) {
				prop.CreateStringProp(&etry->localName, hostbuf, i);
				goto done;
			}
			count++;
		}
	}
	/* if we reach this point, we have not found any domain we should strip. Now
	 * we try and see if the host itself is listed in the -l command line option
	 * and so should be stripped also. If so, we do it and return. Please note that
	 * -l list FQDNs, not just the hostname part. If it did just list the hostname, the
	 * door would be wide-open for all kinds of mixing up of hosts. Because of this,
	 * you'll see comparison against the full string (pszHostFQDN) below.
	 */
	if (glbl.GetLocalHosts() != NULL) {
		count = 0;
		while (glbl.GetLocalHosts()[count]) {
			if (!strcmp((char *)fqdnLower, (char *)glbl.GetLocalHosts()[count])) {
				prop.CreateStringProp(&etry->localName, hostbuf, i);
				goto done;
			}
			count++;
		}
	}

	/* at this point, we have not found anything, so we again use the
	 * already-created complete full name property.
	 */
	prop.AddRef(etry->fqdnLowerCase);
	etry->localName = etry->fqdnLowerCase;
done:
	return;
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
resolveAddr(struct sockaddr_storage *addr, dnscache_entry_t *etry)
{
	DEFiRet;
	int error;
	sigset_t omask, nmask;
	struct addrinfo hints, *res;
	char szIP[80]; /* large enough for IPv6 */
	char fqdnBuf[NI_MAXHOST];
	rs_size_t fqdnLen;
	rs_size_t i;

	error = mygetnameinfo((struct sockaddr *)addr, SALEN((struct sockaddr *)addr),
	    (char *)szIP, sizeof(szIP), NULL, 0, NI_NUMERICHOST);
	if (error) {
		dbgprintf("Malformed from address %s\n", gai_strerror(error));
		ABORT_FINALIZE(RS_RET_INVALID_SOURCE);
	}

	if (!glbl.GetDisableDNS()) {
		sigemptyset(&nmask);
		sigaddset(&nmask, SIGHUP);
		pthread_sigmask(SIG_BLOCK, &nmask, &omask);

		error = mygetnameinfo((struct sockaddr *)addr, SALEN((struct sockaddr *)addr),
		    fqdnBuf, NI_MAXHOST, NULL, 0, NI_NAMEREQD);

		if (error == 0) {
			memset(&hints, 0, sizeof(struct addrinfo));
			hints.ai_flags = AI_NUMERICHOST;

			/* we now do a lookup once again. This one should fail,
			 * because we should not have obtained a non-numeric address. If
			 * we got a numeric one, someone messed with DNS!
			 */
			if (getaddrinfo(fqdnBuf, NULL, &hints, &res) == 0) {
				uchar szErrMsg[1024];
				freeaddrinfo(res);
				/* OK, we know we have evil. The question now is what to do about
				 * it. One the one hand, the message might probably be intended
				 * to harm us. On the other hand, losing the message may also harm us.
				 * Thus, the behaviour is controlled by the $DropMsgsWithMaliciousDnsPTRRecords
				 * option. If it tells us we should discard, we do so, else we proceed,
				 * but log an error message together with it.
				 * time being, we simply drop the name we obtained and use the IP - that one
				 * is OK in any way. We do also log the error message. rgerhards, 2007-07-16
		 		 */
				if (glbl.GetDropMalPTRMsgs() == 1) {
					snprintf((char *)szErrMsg, sizeof(szErrMsg),
					    "Malicious PTR record, message dropped "
					    "IP = \"%s\" HOST = \"%s\"",
					    szIP, fqdnBuf);
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
				snprintf((char *)szErrMsg, sizeof(szErrMsg),
				    "Malicious PTR record (message accepted, but used IP "
				    "instead of PTR name: IP = \"%s\" HOST = \"%s\"",
				    szIP, fqdnBuf);
				errmsg.LogError(0, NO_ERRCODE, "%s", szErrMsg);

				error = 1; /* that will trigger using IP address below. */
			} else {	   /* we have a valid entry, so let's create the respective properties */
				fqdnLen = strlen(fqdnBuf);
				prop.CreateStringProp(&etry->fqdn, (uchar *)fqdnBuf, fqdnLen);
				for (i = 0; i < fqdnLen; ++i)
					fqdnBuf[i] = tolower(fqdnBuf[i]);
				prop.CreateStringProp(&etry->fqdnLowerCase, (uchar *)fqdnBuf, fqdnLen);
			}
		}
		pthread_sigmask(SIG_SETMASK, &omask, NULL);
	}


finalize_it:
	if (iRet != RS_RET_OK) {
		strcpy(szIP, "?error.obtaining.ip?");
		error = 1; /* trigger hostname copies below! */
	}

	/* we need to create the inputName property (only once during our lifetime) */
	prop.CreateStringProp(&etry->ip, (uchar *)szIP, strlen(szIP));

	if (error || glbl.GetDisableDNS()) {
		dbgprintf("Host name for your address (%s) unknown\n", szIP);
		prop.AddRef(etry->ip);
		etry->fqdn = etry->ip;
		prop.AddRef(etry->ip);
		etry->fqdnLowerCase = etry->ip;
	}

	setLocalHostName(etry);

	RETiRet;
}


static rsRetVal
addEntry(struct sockaddr_storage *addr, dnscache_entry_t **pEtry)
{
	int r;
	struct sockaddr_storage *keybuf;
	dnscache_entry_t *etry = NULL;
	DEFiRet;

	CHKmalloc(etry = MALLOC(sizeof(dnscache_entry_t)));
	CHKiRet(resolveAddr(addr, etry));
	memcpy(&etry->addr, addr, SALEN((struct sockaddr *)addr));
	etry->nUsed = 0;
	*pEtry = etry;

	CHKmalloc(keybuf = malloc(sizeof(struct sockaddr_storage)));
	memcpy(keybuf, addr, sizeof(struct sockaddr_storage));

	pthread_rwlock_unlock(&dnsCache.rwlock); /* release read lock */
	pthread_rwlock_wrlock(&dnsCache.rwlock); /* and re-aquire for writing */
	r = hashtable_insert(dnsCache.ht, keybuf, *pEtry);
	if (r == 0) {
		DBGPRINTF("dnscache: inserting element failed\n");
	}
	pthread_rwlock_unlock(&dnsCache.rwlock);
	pthread_rwlock_rdlock(&dnsCache.rwlock); /* we need this again */

finalize_it:
	if (iRet != RS_RET_OK && etry != NULL) {
		/* Note: sub-fields cannot be populated in this case */
		free(etry);
	}
	RETiRet;
}


/* validate if an entry is still valid and, if not, re-query it.
 * In the initial implementation, this is a dummy!
 * TODO: implement!
 */
static inline rsRetVal
    validateEntry(dnscache_entry_t __attribute__((unused)) * etry, struct sockaddr_storage __attribute__((unused)) * addr)
{
	return RS_RET_OK;
}


/* This is the main function: it looks up an entry and returns it's name
 * and IP address. If the entry is not yet inside the cache, it is added.
 * If the entry can not be resolved, an error is reported back. If fqdn
 * or fqdnLowerCase are NULL, they are not set.
 */
rsRetVal
dnscacheLookup(struct sockaddr_storage *addr, prop_t **fqdn, prop_t **fqdnLowerCase,
    prop_t **localName, prop_t **ip)
{
	dnscache_entry_t *etry;
	DEFiRet;

	pthread_rwlock_rdlock(&dnsCache.rwlock); /* TODO: optimize this! */
	etry = findEntry(addr);
	dbgprintf("dnscache: entry %p found\n", etry);
	if (etry == NULL) {
		CHKiRet(addEntry(addr, &etry));
	} else {
		CHKiRet(validateEntry(etry, addr));
	}
	prop.AddRef(etry->ip);
	*ip = etry->ip;
	if (fqdn != NULL) {
		prop.AddRef(etry->fqdn);
		*fqdn = etry->fqdn;
	}
	if (fqdnLowerCase != NULL) {
		prop.AddRef(etry->fqdnLowerCase);
		*fqdnLowerCase = etry->fqdnLowerCase;
	}
	if (localName != NULL) {
		prop.AddRef(etry->localName);
		*localName = etry->localName;
	}

finalize_it:
	pthread_rwlock_unlock(&dnsCache.rwlock);
	if (iRet != RS_RET_OK && iRet != RS_RET_ADDRESS_UNKNOWN) {
		DBGPRINTF("dnscacheLookup failed with iRet %d\n", iRet);
		prop.AddRef(staticErrValue);
		*ip = staticErrValue;
		if (fqdn != NULL) {
			prop.AddRef(staticErrValue);
			*fqdn = staticErrValue;
		}
		if (fqdnLowerCase != NULL) {
			prop.AddRef(staticErrValue);
			*fqdnLowerCase = staticErrValue;
		}
		if (localName != NULL) {
			prop.AddRef(staticErrValue);
			*localName = staticErrValue;
		}
	}
	RETiRet;
}
