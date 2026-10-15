/** SPDX-License-Identifier: Apache-2.0
 * @file
 * @brief Control-thread ownership of a named policy registry, independent of syntax snapshots.
 *
 * No message-path reference operations or locks are required: bindings change
 * only under the listener fence. Every limiter must be destroyed before its
 * binding releases the owner. A borrowed startup registry is never inspected
 * or destroyed when its final owner reference disappears.
 */
#ifndef IMTCP_POLICY_OWNER_H_INCLUDED
#define IMTCP_POLICY_OWNER_H_INCLUDED

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "rsyslog.h"
#include "rsconf.h"
#include "ratelimit.h"

/** @brief Registry lifetime shared by metadata, prepared and runtime bindings. */
typedef struct imtcpPolicyOwner_s {
    size_t references; /**< Control-thread references, never message references. */
    void *registry; /**< Borrowed startup configuration or owned private configuration. */
    void (*destroy)(void *); /**< NULL for borrowed startup registry. */
} imtcpPolicyOwner_t;

/** @brief Allocate a registry owner; registry ownership transfers only on success. */
static imtcpPolicyOwner_t *imtcpPolicyOwnerNew(void *registry, void (*destroy)(void *)) {
    imtcpPolicyOwner_t *owner = calloc(1, sizeof(*owner));
    if (owner != NULL) {
        owner->references = 1;
        owner->registry = registry;
        owner->destroy = destroy;
    }
    return owner;
}

/** @brief Retain a control-thread owner for one independently released holder. */
static imtcpPolicyOwner_t *imtcpPolicyOwnerRetain(imtcpPolicyOwner_t *owner) {
    if (owner != NULL) {
        assert(owner->references != 0 && owner->references != SIZE_MAX);
        ++owner->references;
    }
    return owner;
}

/** @brief Clear one holder, destroying an owned registry only after its last consumer. */
static void imtcpPolicyOwnerRelease(imtcpPolicyOwner_t **holder) {
    imtcpPolicyOwner_t *owner;
    if (holder == NULL || *holder == NULL) return;
    owner = *holder;
    *holder = NULL;
    assert(owner->references != 0);
    if (--owner->references == 0) {
        if (owner->destroy != NULL) owner->destroy(owner->registry);
        free(owner);
    }
}

/** @brief Transfer prepared/runtime references without allocating at publication. */
static void imtcpPolicyOwnerPublish(imtcpPolicyOwner_t **runtime,
                                    imtcpPolicyOwner_t **prepared,
                                    imtcpPolicyOwner_t **retired) {
    assert(runtime != NULL && prepared != NULL && retired != NULL && *retired == NULL);
    *retired = *runtime;
    *runtime = *prepared;
    *prepared = NULL;
}

/** @brief Owned source policy values; only module-owned maps attach registry owners. */
typedef struct reloadRateProfile_s {
    char *name;
    char *fingerprint;
    unsigned interval;
    unsigned burst;
    int severity;
    int simple;
    imtcpPolicyOwner_t *owner;
    struct reloadRateProfile_s *next;
} reloadRateProfile_t;


static reloadRateProfile_t *reloadRateFind(reloadRateProfile_t *profiles, const char *name) {
    if (name == NULL) return NULL;
    for (; profiles != NULL; profiles = profiles->next)
        if (!strcmp(profiles->name, name)) return profiles;
    return NULL;
}

static void reloadRateProfilesDestruct(reloadRateProfile_t **head) {
    while (*head != NULL) {
        reloadRateProfile_t *profile = *head;
        *head = profile->next;
        imtcpPolicyOwnerRelease(&profile->owner);
        free(profile->name);
        free(profile->fingerprint);
        free(profile);
    }
}


static void destroyPrivateRateRegistry(void *registry) {
    rsconf_t *config = registry;
    ratelimit_cfgsDestruct(&config->ratelimit_cfgs);
    free(config);
}

/** @brief Prepare a complete metadata map, sharing unchanged policy owners across generations.
 * The map deep-owns strings and preserves unused declarations. Startup owners
 * borrow runConf; new simple policies own a registry containing copied scalars
 * and a copied name only (no templates, syntax, files or watcher callbacks).
 */
static rsRetVal prepareReloadPolicies(reloadRateProfile_t *oldProfiles,
                                      reloadRateProfile_t *newProfiles,
                                      reloadRateProfile_t *activeProfiles,
                                      rsconf_t *startup,
                                      reloadRateProfile_t **out) {
    reloadRateProfile_t *head = NULL;
    reloadRateProfile_t *copy = NULL;
    rsconf_t *registry = NULL;
    DEFiRet;
    for (reloadRateProfile_t *source = newProfiles; source != NULL; source = source->next) {
        reloadRateProfile_t *old = reloadRateFind(oldProfiles, source->name);
        reloadRateProfile_t *active = reloadRateFind(activeProfiles, source->name);
        CHKmalloc(copy = calloc(1, sizeof(*copy)));
        CHKmalloc(copy->name = strdup(source->name));
        CHKmalloc(copy->fingerprint = strdup(source->fingerprint));
        copy->simple = source->simple;
        copy->interval = source->interval;
        copy->burst = source->burst;
        copy->severity = source->severity;
        if (old != NULL && !strcmp(old->fingerprint, source->fingerprint)) {
            if (active != NULL) {
                if (strcmp(active->fingerprint, source->fingerprint) || active->owner == NULL)
                    ABORT_FINALIZE(RS_RET_INTERNAL_ERROR);
                copy->owner = imtcpPolicyOwnerRetain(active->owner);
            } else {
                if (activeProfiles != NULL) ABORT_FINALIZE(RS_RET_INTERNAL_ERROR);
                CHKmalloc(copy->owner = imtcpPolicyOwnerNew(startup, NULL));
            }
        } else {
            if (!copy->simple) ABORT_FINALIZE(RS_RET_NOT_IMPLEMENTED);
            CHKmalloc(registry = calloc(1, sizeof(*registry)));
            CHKiRet(ratelimit_cfgsInit(&registry->ratelimit_cfgs));
            CHKiRet(ratelimitAddConfig(registry, copy->name, copy->interval, copy->burst, copy->severity, NULL, RSFALSE,
                                       NULL, RSFALSE, NULL, NULL, 0, 0, RSTRUE, RSFALSE));
            CHKmalloc(copy->owner = imtcpPolicyOwnerNew(registry, destroyPrivateRateRegistry));
            registry = NULL;
        }
        copy->next = head;
        head = copy;
        copy = NULL;
    }
    *out = head;
    head = NULL;
finalize_it:
    if (registry != NULL) destroyPrivateRateRegistry(registry);
    reloadRateProfilesDestruct(&copy);
    reloadRateProfilesDestruct(&head);
    RETiRet;
}


/** @brief One policy binding's prepared/retired resources and publication-only slots. */
typedef struct imtcpPolicyBinding_s {
    imtcpPolicyOwner_t **runtimeOwner; /**< Borrowed slot, cleared at publication. */
    ratelimit_t **runtimeLimiter; /**< Borrowed slot, never read during destruction. */
    uchar **runtimeName; /**< Borrowed listener-owned name slot. */
    imtcpPolicyOwner_t *preparedPolicyOwner;
    imtcpPolicyOwner_t *retiredPolicyOwner;
    ratelimit_t *preparedRateLimiter;
    ratelimit_t *retiredRateLimiter;
    uchar *preparedRatelimitName;
    uchar *retiredRatelimitName;
    int swapRateLimiter;
} imtcpPolicyBinding_t;

/** @brief Own all policy resources independently of listener and source-snapshot lifetime. */
typedef struct imtcpPolicyTransaction_s {
    size_t count;
    reloadRateProfile_t *preparedPolicies;
    reloadRateProfile_t *retiredPolicies;
    imtcpPolicyBinding_t entries[];
} imtcpPolicyTransaction_t;

/** @brief Unwind prepared or retire published policy resources; never access runtime slots. */
static void imtcpPolicyTransactionDestruct(imtcpPolicyTransaction_t **holder) {
    imtcpPolicyTransaction_t *state;
    if (holder == NULL || *holder == NULL) return;
    state = *holder;
    *holder = NULL;
    /* Destroy every limiter before dropping any registry-owner reference. */
    for (size_t i = 0; i < state->count; ++i) {
        if (state->entries[i].preparedRateLimiter != NULL) ratelimitDestruct(state->entries[i].preparedRateLimiter);
        if (state->entries[i].retiredRateLimiter != NULL) ratelimitDestruct(state->entries[i].retiredRateLimiter);
        free(state->entries[i].preparedRatelimitName);
        free(state->entries[i].retiredRatelimitName);
    }
    for (size_t i = 0; i < state->count; ++i) {
        imtcpPolicyOwnerRelease(&state->entries[i].preparedPolicyOwner);
        imtcpPolicyOwnerRelease(&state->entries[i].retiredPolicyOwner);
    }
    reloadRateProfilesDestruct(&state->preparedPolicies);
    reloadRateProfilesDestruct(&state->retiredPolicies);
    free(state);
}

/** @brief Prepare an owned complete map without changing active metadata or buckets. */
static rsRetVal imtcpPolicyTransactionPrepare(reloadRateProfile_t *oldProfiles,
                                              reloadRateProfile_t *newProfiles,
                                              reloadRateProfile_t *activeProfiles,
                                              rsconf_t *startup,
                                              size_t count,
                                              imtcpPolicyTransaction_t **out) {
    imtcpPolicyTransaction_t *state = NULL;
    DEFiRet;
    if (out == NULL || *out != NULL || startup == NULL) return RS_RET_PARAM_ERROR;
    if (count > (SIZE_MAX - sizeof(*state)) / sizeof(state->entries[0])) return RS_RET_OUT_OF_MEMORY;
    CHKmalloc(state = calloc(1, sizeof(*state) + count * sizeof(state->entries[0])));
    state->count = count;
    CHKiRet(prepareReloadPolicies(oldProfiles, newProfiles, activeProfiles, startup, &state->preparedPolicies));
    *out = state;
    state = NULL;
finalize_it:
    imtcpPolicyTransactionDestruct(&state);
    RETiRet;
}

/** @brief Reserve one limiter/name and owner binding, borrowing slots only until publication.
 * All slots must remain valid until commit under the listener fence. Abort,
 * destruction and post-resume retirement never dereference the borrowed slots.
 */
static rsRetVal imtcpPolicyBindingPrepare(imtcpPolicyBinding_t *entry,
                                          reloadRateProfile_t *oldProfiles,
                                          reloadRateProfile_t *policies,
                                          imtcpPolicyOwner_t **runtimeOwner,
                                          ratelimit_t **runtimeLimiter,
                                          uchar **runtimeName,
                                          const char *name,
                                          unsigned interval,
                                          unsigned burst) {
    reloadRateProfile_t *profile = reloadRateFind(policies, name);
    reloadRateProfile_t *old;
    DEFiRet;
    if (entry == NULL || runtimeOwner == NULL || runtimeLimiter == NULL || runtimeName == NULL)
        return RS_RET_PARAM_ERROR;
    old = reloadRateFind(oldProfiles, (const char *)*runtimeName);
    if (name != NULL && (profile == NULL || profile->owner == NULL)) return RS_RET_NOT_FOUND;
    entry->runtimeOwner = runtimeOwner;
    entry->runtimeLimiter = runtimeLimiter;
    entry->runtimeName = runtimeName;
    entry->preparedPolicyOwner = imtcpPolicyOwnerRetain(profile == NULL ? NULL : profile->owner);
    if ((name == NULL && *runtimeName == NULL) ||
        (name != NULL && *runtimeName != NULL && !strcmp(name, (const char *)*runtimeName) && old != NULL &&
         !strcmp(old->fingerprint, profile->fingerprint)))
        FINALIZE;
    if (name != NULL) {
        CHKmalloc(entry->preparedRatelimitName = (uchar *)strdup(name));
        CHKiRet(ratelimitNewFromConfig(&entry->preparedRateLimiter, profile->owner->registry, name, "tcpsrv", NULL));
    } else {
        CHKiRet(ratelimitNew(&entry->preparedRateLimiter, "tcpsrv", NULL));
        ratelimitSetLinuxLike(entry->preparedRateLimiter, interval, burst);
    }
    ratelimitSetThreadSafe(entry->preparedRateLimiter);
    entry->swapRateLimiter = 1;
finalize_it:
    RETiRet;
}

/** @brief Infallibly transfer complete maps and listener bindings with every listener fenced. */
static void imtcpPolicyTransactionCommit(imtcpPolicyTransaction_t *state, reloadRateProfile_t **active) {
    state->retiredPolicies = *active;
    *active = state->preparedPolicies;
    state->preparedPolicies = NULL;
    for (size_t i = 0; i < state->count; ++i) {
        imtcpPolicyBinding_t *entry = &state->entries[i];
        assert(entry->runtimeOwner != NULL && entry->runtimeLimiter != NULL && entry->runtimeName != NULL);
        if (entry->swapRateLimiter) {
            entry->retiredRateLimiter = *entry->runtimeLimiter;
            *entry->runtimeLimiter = entry->preparedRateLimiter;
            entry->preparedRateLimiter = NULL;
            entry->retiredRatelimitName = *entry->runtimeName;
            *entry->runtimeName = entry->preparedRatelimitName;
            entry->preparedRatelimitName = NULL;
        }
        imtcpPolicyOwnerPublish(entry->runtimeOwner, &entry->preparedPolicyOwner, &entry->retiredPolicyOwner);
        entry->runtimeOwner = NULL;
        entry->runtimeLimiter = NULL;
        entry->runtimeName = NULL;
    }
}

/** @brief Consume policy state only after successful module resume; errors retain every resource. */
static rsRetVal imtcpPolicyTransactionRetire(imtcpPolicyTransaction_t **state, rsRetVal resumeResult) {
    if (resumeResult != RS_RET_OK) return resumeResult;
    imtcpPolicyTransactionDestruct(state);
    return RS_RET_OK;
}

#endif
