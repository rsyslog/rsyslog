/* dnscache.c
 * Implementation of a real DNS cache
 *
 * File begun on 2011-06-06 by RGerhards
 * The initial implementation is far from being optimal. The idea is to
 * first get somethting that'S functionally OK, and then evolve the algorithm.
 * In any case, even the initial implementaton is far faster than what we had
 * before. -- rgerhards, 2011-06-06
 *
 * Copyright 2011-2019 by Rainer Gerhards and Adiscon GmbH.
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
#include "rsconf.h"

/* module data structures */
struct dnscache_entry_s {
    struct sockaddr_storage addr;
    prop_t *fqdn;
    prop_t *fqdnLowerCase;
    prop_t *localName; /* only local name, without domain part (if configured so) */
    prop_t *ip;
    time_t validUntil;
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
DEFobjStaticHelpers
DEFobjCurrIf(glbl)
DEFobjCurrIf(prop)
static dnscache_t dnsCache;
static prop_t *staticErrValue;


/* Our hash function.
 */
static unsigned int
hash_from_key_fn(void *k)
{
    int len = 0;
    uchar *rkey; /* we treat this as opaque bytes */
    unsigned hashval = 1;

    switch (((struct sockaddr *)k)->sa_family) {
        case AF_INET:
            len = sizeof (struct in_addr);
            rkey = (uchar*) &(((struct sockaddr_in *)k)->sin_addr);
            break;
        case AF_INET6:
            len = sizeof (struct in6_addr);
            rkey = (uchar*) &(((struct sockaddr_in6 *)k)->sin6_addr);
            break;
        default:
            dbgprintf("hash_from_key_fn: unknown address family!\n");
            len = 0;
            rkey = NULL;
    }
    while(len--) {
        /* the casts are done in order to prevent undefined behavior sanitizer
         * from triggering warnings. Actually, it would be OK if we have just
         * "random" truncation.
         */
        hashval = (unsigned) (hashval * (unsigned long long) 33) + *rkey++;
    }

    return hashval;
}


static int
key_equals_fn(void *key1, void *key2)
{
    int RetVal = 0;

    if(((struct sockaddr *)key1)->sa_family != ((struct sockaddr *)key2)->sa_family) {
        return 0;
    }
    switch (((struct sockaddr *)key1)->sa_family) {
        case AF_INET:
            RetVal = !memcmp(&((struct sockaddr_in *)key1)->sin_addr,
                &((struct sockaddr_in *)key2)->sin_addr, sizeof (struct in_addr));
            break;
        case AF_INET6:
            RetVal = !memcmp(&((struct sockaddr_in6 *)key1)->sin6_addr,
                &((struct sockaddr_in6 *)key2)->sin6_addr, sizeof (struct in6_addr));
            break;
        default:
            // No action needed for other cases
            break;
    }

    return RetVal;
}

/* destruct a cache entry.
 * Precondition: entry must already be unlinked from list
 */
static void ATTR_NONNULL()
entryDestruct(dnscache_entry_t *const etry)
{
    if(etry->fqdn != NULL)
        prop.Destruct(&etry->fqdn);
    if(etry->fqdnLowerCase != NULL)
        prop.Destruct(&etry->fqdnLowerCase);
    if(etry->localName != NULL)
        prop.Destruct(&etry->localName);
    if(etry->ip != NULL)
        prop.Destruct(&etry->ip);
    free(etry);
}

/* init function (must be called once) */
rsRetVal
dnscacheInit(void)
{
    DEFiRet;
    if((dnsCache.ht = create_hashtable(100, hash_from_key_fn, key_equals_fn,
                (void(*)(void*))entryDestruct)) == NULL) {
        DBGPRINTF("dnscache: error creating hash table!\n");
        ABORT_FINALIZE(RS_RET_ERR); // TODO: make this degrade, but run!
    }
    dnsCache.nEntries = 0;
    pthread_rwlock_init(&dnsCache.rwlock, NULL);
    CHKiRet(objGetObjInterface(&obj)); /* this provides the root pointer for all other queries */
    CHKiRet(objUse(glbl, CORE_COMPONENT));
    CHKiRet(objUse(prop, CORE_COMPONENT));

    prop.Construct(&staticErrValue);
    prop.SetString(staticErrValue, (uchar*)"???", 3);
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
    objRelease(prop, CORE_COMPONENT);
    RETiRet;
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
    int i;
    uchar hostbuf[NI_MAXHOST];

    if(glbl.GetPreserveFQDN()) {
        prop.AddRef(etry->fqdnLowerCase);
        etry->localName = etry->fqdnLowerCase;
        goto done;
    }

    /* strip domain, if configured for this entry */
    fqdnLower = propGetSzStr(etry->fqdnLowerCase);
    p = (uchar*)strchr((char*)fqdnLower, '.'); /* find start of domain name "machine.example.com" */
    if(p == NULL) { /* do we have a domain part? */
        prop.AddRef(etry->fqdnLowerCase); /* no! */
        etry->localName = etry->fqdnLowerCase;
        goto done;
    }

    i = p - fqdnLower; /* length of hostname */
    memcpy(hostbuf, fqdnLower, i);
    hostbuf[i] = '\0';

    /* at this point, we have not found anything, so we again use the
     * already-created complete full name property.
     */
    prop.AddRef(etry->fqdnLowerCase);
    etry->localName = etry->fqdnLowerCase;
done:   return;
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
static rsRetVal ATTR_NONNULL()
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
                (char*) szIP, sizeof(szIP), NULL, 0, NI_NUMERICHOST);
    if(error) {
        dbgprintf("Malformed from address %s\n", gai_strerror(error));
        ABORT_FINALIZE(RS_RET_INVALID_SOURCE);
    }

    if(!glbl.GetDisableDNS(runConf)) {
        sigemptyset(&nmask);
        sigaddset(&nmask, SIGHUP);
        pthread_sigmask(SIG_BLOCK, &nmask, &omask);

        error = mygetnameinfo((struct sockaddr *)addr, SALEN((struct sockaddr *) addr),
                    fqdnBuf, NI_MAXHOST, NULL, 0, NI_NAMEREQD);

        if(error == 0) {
            memset (&hints, 0, sizeof (struct addrinfo));
            hints.ai_flags = AI_NUMERICHOST;

            /* we now do a lookup once again. This one should fail,
             * because we should not have obtained a non-numeric address. If
             * we got a numeric one, someone messed with DNS!
             */
            if(getaddrinfo (fqdnBuf, NULL, &hints, &res) == 0) {
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
                if(glbl.GetDropMalPTRMsgs(runConf) == 1) {
                    LogError(0, RS_RET_MALICIOUS_ENTITY,
                         "Malicious PTR record, message dropped "
                         "IP = \"%s\" HOST = \"%s\"",
                         szIP, fqdnBuf);
                    pthread_sigmask(SIG_SETMASK, &omask, NULL);
                    ABORT_FINALIZE(RS_RET_MALICIOUS_ENTITY);
                }

                /* Please note: we deal with a malicous entry. Thus, we have crafted
                 * the snprintf() below so that all text is in front of the entry - maybe
                 * it contains characters that make the message unreadable
                 * (OK, I admit this is more or less impossible, but I am paranoid...)
                 * rgerhards, 2007-07-16
                 */
                LogError(0, NO_ERRCODE,
                     "Malicious PTR record (message accepted, but used IP "
                     "instead of PTR name: IP = \"%s\" HOST = \"%s\"",
                     szIP, fqdnBuf);

                error = 1; /* that will trigger using IP address below. */
            } else {/* we have a valid entry, so let's create the respective properties */
                fqdnLen = strlen(fqdnBuf);
                prop.CreateStringProp(&etry->fqdn, (uchar*)fqdnBuf, fqdnLen);
                for(i = 0 ; i < fqdnLen ; ++i)
                    fqdnBuf[i] = tolower(fqdnBuf[i]);
                prop.CreateStringProp(&etry->fqdnLowerCase, (uchar*)fqdnBuf, fqdnLen);
            }
        }
        pthread_sigmask(SIG_SETMASK, &omask, NULL);
    }


finalize_it:
    if(iRet != RS_RET_OK) {
        strcpy(szIP, "?error.obtaining.ip?");
        error = 1; /* trigger hostname copies below! */
    }

    prop.CreateStringProp(&etry->ip, (uchar*)szIP, strlen(szIP));

    if(error || glbl.GetDisableDNS(runConf)) {
        dbgprintf("Host name for your address (%s) unknown\n", szIP);
        prop.AddRef(etry->ip);
        etry->fqdn = etry->ip;
        prop.AddRef(etry->ip);
        etry->fqdnLowerCase = etry->ip;
    }

    setLocalHostName(etry);

    RETiRet;
}


static rsRetVal ATTR_NONNULL()
addEntry(struct sockaddr_storage *const addr, dnscache_entry_t **const pEtry)
{
    int r;
    dnscache_entry_t *etry = NULL;
    DEFiRet;

    /* entry still does not exist, so add it */
    struct sockaddr_storage *const keybuf =  malloc(sizeof(struct sockaddr_storage));
    CHKmalloc(keybuf);
    CHKmalloc(etry = malloc(sizeof(dnscache_entry_t)));
    resolveAddr(addr, etry);
    assert(etry != NULL);
    memcpy(&etry->addr, addr, SALEN((struct sockaddr*) addr));
    etry->nUsed = 0;
    if(runConf->globals.dnscacheEnableTTL) {
        etry->validUntil = time(NULL) + runConf->globals.dnscacheDefaultTTL;
    }

    memcpy(keybuf, addr, sizeof(struct sockaddr_storage));

    r = hashtable_insert(dnsCache.ht, keybuf, etry);
    if(r == 0) {
        DBGPRINTF("dnscache: inserting element failed\n");
    }
    *pEtry = etry;

finalize_it:
    if(iRet != RS_RET_OK) {
        free(keybuf);
    }
    RETiRet;
}


static rsRetVal ATTR_NONNULL(1, 5)
findEntry(struct sockaddr_storage *const addr,
    prop_t **const fqdn, prop_t **const fqdnLowerCase,
    prop_t **const localName, prop_t **const ip)
{
    DEFiRet;

    pthread_rwlock_rdlock(&dnsCache.rwlock);
    dnscache_entry_t * etry = hashtable_search(dnsCache.ht, addr);
    DBGPRINTF("findEntry: 1st lookup found %p\n", etry);

    if(etry == NULL || (runConf->globals.dnscacheEnableTTL && (etry->validUntil <= time(NULL)))) {
        pthread_rwlock_unlock(&dnsCache.rwlock);
        pthread_rwlock_wrlock(&dnsCache.rwlock);
        etry = hashtable_search(dnsCache.ht, addr); /* re-query, might have changed */
        DBGPRINTF("findEntry: 2nd lookup found %p\n", etry);
        if(etry == NULL || (runConf->globals.dnscacheEnableTTL && (etry->validUntil <= time(NULL)))) {
            if(etry != NULL) {
                DBGPRINTF("hashtable: entry timed out, discarding it; "
                    "valid until %lld, now %lld\n",
                    (long long) etry->validUntil, (long long) time(NULL));
                dnscache_entry_t *const deleted = hashtable_remove(dnsCache.ht, addr);
                if(deleted != etry) {
                    LogError(0, RS_RET_INTERNAL_ERROR, "dnscache %d: removed different "
                        "hashtable entry than expected - please report issue; "
                        "rsyslog version is %s", __LINE__, VERSION);
                }
                entryDestruct(etry);
            }
            /* now entry doesn't exist in any case, so let's (re)create it */
            CHKiRet(addEntry(addr, &etry));
        }
    }

    prop.AddRef(etry->ip);
    *ip = etry->ip;
    if(fqdn != NULL) {
        prop.AddRef(etry->fqdn);
        *fqdn = etry->fqdn;
    }
    if(fqdnLowerCase != NULL) {
        prop.AddRef(etry->fqdnLowerCase);
        *fqdnLowerCase = etry->fqdnLowerCase;
    }
    if(localName != NULL) {
        prop.AddRef(etry->localName);
        *localName = etry->localName;
    }

finalize_it:
    pthread_rwlock_unlock(&dnsCache.rwlock);
    RETiRet;
}


/* This is the main function: it looks up an entry and returns it's name
 * and IP address. If the entry is not yet inside the cache, it is added.
 * If the entry can not be resolved, an error is reported back. If fqdn
 * or fqdnLowerCase are NULL, they are not set.
 */
rsRetVal ATTR_NONNULL(1, 5)
dnscacheLookup(struct sockaddr_storage *const addr,
    prop_t **const fqdn, prop_t **const fqdnLowerCase,
    prop_t **const localName, prop_t **const ip)
{
    DEFiRet;

    iRet = findEntry(addr, fqdn, fqdnLowerCase, localName, ip);

    if(iRet != RS_RET_OK) {
        DBGPRINTF("dnscacheLookup failed with iRet %d\n", iRet);
        prop.AddRef(staticErrValue);
        *ip = staticErrValue;
        if(fqdn != NULL) {
            prop.AddRef(staticErrValue);
            *fqdn = staticErrValue;
        }
        if(fqdnLowerCase != NULL) {
            prop.AddRef(staticErrValue);
            *fqdnLowerCase = staticErrValue;
        }
        if(localName != NULL) {
            prop.AddRef(staticErrValue);
            *localName = staticErrValue;
        }
    }
    RETiRet;
}
