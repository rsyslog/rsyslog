/** SPDX-License-Identifier: Apache-2.0
 * @file
 * @brief Exercise shared production policy transaction helpers without sockets or linker GC.
 *
 * Registry and limiter backends below count live consumers and assert that
 * destruction never precedes their release. Real production map, binding and
 * retirement-result gates run directly; failed resume must retain the transaction. Two
 * A listeners plus B expose split sharing and premature registry retirement.
 */
#include "config.h"
#undef NDEBUG
#include <stdio.h>
#include "rsyslog.h"
#include "ruleset.h"
#include "tcpsrv.h"
#include "rsconf.h"
#include "../../plugins/imtcp/imtcp-policy-owner.h"

/* Only the registry backend is replaced; production transaction helpers below
 * are compiled unchanged on every supported compiler/linker. */
struct modConfData_s {
    rsconf_t *pConf;
    reloadRateProfile_t *reloadRateProfiles;
};
typedef struct {
    tcpsrv_t *tcpsrv;
    imtcpPolicyOwner_t *policyOwner;
} tcpsrv_etry_t;
static reloadRateProfile_t *activeRateProfiles;
#undef NDEBUG
#include <assert.h>

#define CHECK(condition)                                            \
    do {                                                            \
        if (!(condition)) {                                         \
            fprintf(stderr, "line %d: %s\n", __LINE__, #condition); \
            return 1;                                               \
        }                                                           \
    } while (0)

typedef struct fakePolicy_s {
    ratelimit_shared_t shared;
    struct fakePolicy_s *next;
} fakePolicy_t;
static int registriesDestroyed;
static int liveLimiters;
static int failResume;

rsRetVal ratelimit_cfgsInit(ratelimit_cfgs_t *cfgs) {
    memset(cfgs, 0, sizeof(*cfgs));
    return RS_RET_OK;
}

void ratelimit_cfgsDestruct(ratelimit_cfgs_t *cfgs) {
    fakePolicy_t *policy = (void *)cfgs->shards[0].ht;
    while (policy != NULL) {
        fakePolicy_t *next = policy->next;
        assert(policy->shared.refcnt == 1);
        free(policy->shared.name);
        free(policy);
        policy = next;
    }
    cfgs->shards[0].ht = NULL;
    ++registriesDestroyed;
}

rsRetVal ratelimitAddConfig(rsconf_t *conf,
                            const char *name,
                            unsigned interval,
                            unsigned burst,
                            int severity,
                            const char *policyFile,
                            sbool watch,
                            const char *debounce,
                            sbool perSource,
                            const char *sourceFile,
                            const char *keyTemplate,
                            unsigned maxStates,
                            unsigned topn,
                            sbool inlineParams,
                            sbool legacy) {
    assert(policyFile == NULL && !watch && debounce == NULL && !perSource && sourceFile == NULL &&
           keyTemplate == NULL && maxStates == 0 && topn == 0 && inlineParams && !legacy);
    fakePolicy_t *policy = calloc(1, sizeof(*policy));
    if (policy == NULL) return RS_RET_OUT_OF_MEMORY;
    policy->shared.name = strdup(name);
    if (policy->shared.name == NULL) {
        free(policy);
        return RS_RET_OUT_OF_MEMORY;
    }
    policy->shared.refcnt = 1;
    policy->shared.interval = interval;
    policy->shared.burst = burst;
    policy->shared.severity = severity;
    policy->next = (void *)conf->ratelimit_cfgs.shards[0].ht;
    conf->ratelimit_cfgs.shards[0].ht = (void *)policy;
    return RS_RET_OK;
}

rsRetVal ratelimitNewFromConfig(
    ratelimit_t **out, rsconf_t *conf, const char *name, const char *module, const char *dynamic) {
    (void)module;
    (void)dynamic;
    for (fakePolicy_t *policy = (void *)conf->ratelimit_cfgs.shards[0].ht; policy != NULL; policy = policy->next) {
        if (strcmp(policy->shared.name, name)) continue;
        *out = calloc(1, sizeof(**out));
        if (*out == NULL) return RS_RET_OUT_OF_MEMORY;
        (*out)->pShared = &policy->shared;
        ++policy->shared.refcnt;
        ++liveLimiters;
        return RS_RET_OK;
    }
    return RS_RET_NOT_FOUND;
}

rsRetVal ratelimitNew(ratelimit_t **out, const char *module, const char *dynamic) {
    (void)module;
    (void)dynamic;
    *out = calloc(1, sizeof(**out));
    if (*out == NULL) return RS_RET_OUT_OF_MEMORY;
    (*out)->pShared = calloc(1, sizeof(*(*out)->pShared));
    if ((*out)->pShared == NULL) {
        free(*out);
        *out = NULL;
        return RS_RET_OUT_OF_MEMORY;
    }
    (*out)->bOwnsShared = 1;
    (*out)->pShared->refcnt = 1;
    ++liveLimiters;
    return RS_RET_OK;
}
void ratelimitSetLinuxLike(ratelimit_t *limiter, unsigned interval, unsigned burst) {
    limiter->pShared->interval = interval;
    limiter->pShared->burst = burst;
}
void ratelimitSetThreadSafe(ratelimit_t *limiter) {
    (void)limiter;
}
void ratelimitDestruct(ratelimit_t *limiter) {
    assert(limiter != NULL && limiter->pShared->refcnt != 0);
    --limiter->pShared->refcnt;
    if (limiter->bOwnsShared) {
        assert(limiter->pShared->refcnt == 0);
        free(limiter->pShared);
    }
    --liveLimiters;
    free(limiter);
}
static void commitState(imtcpPolicyTransaction_t *state) {
    imtcpPolicyTransactionCommit(state, &activeRateProfiles);
}
static void abortState(imtcpPolicyTransaction_t *state) {
    imtcpPolicyTransactionDestruct(&state);
}
static rsRetVal retireState(imtcpPolicyTransaction_t *state) {
    return imtcpPolicyTransactionRetire(&state, failResume ? RS_RET_RETRY : RS_RET_OK);
}

static imtcpPolicyTransaction_t *prepareState(modConfData_t *oldConfig,
                                              modConfData_t *newConfig,
                                              tcpsrv_etry_t *endpoints,
                                              const char **names) {
    imtcpPolicyTransaction_t *state = NULL;
    if (imtcpPolicyTransactionPrepare(oldConfig->reloadRateProfiles, newConfig->reloadRateProfiles, activeRateProfiles,
                                      newConfig->pConf, 3, &state) != RS_RET_OK)
        return NULL;
    for (size_t i = 0; i < 3; ++i) {
        tcpLstnPortList_t *listener = endpoints[i].tcpsrv->pLstnPorts;
        if (imtcpPolicyBindingPrepare(&state->entries[i], oldConfig->reloadRateProfiles, state->preparedPolicies,
                                      &endpoints[i].policyOwner, &listener->ratelimiter,
                                      &listener->cnf_params->pszRatelimitName, names[i], 0, 10000) != RS_RET_OK)
            goto failed;
    }
    return state;
failed:
    imtcpPolicyTransactionDestruct(&state);
    return NULL;
}

int main(void) {
    rsconf_t startup = {0};
    char nameA[] = "A", nameB[] = "B", a1[] = "A1", b1[] = "B1", a2[] = "A2", b2[] = "B2";
    reloadRateProfile_t a = {
        .name = nameA, .fingerprint = a1, .simple = 1, .interval = 3600, .burst = 2, .severity = 7};
    reloadRateProfile_t b = {.name = nameB, .fingerprint = b1, .simple = 1, .burst = 3, .severity = -1};
    a.next = &b;
    modConfData_t oldConfig = {.pConf = &startup, .reloadRateProfiles = &a};
    reloadRateProfile_t newA = a, newB = b;
    newA.next = &newB;
    newB.fingerprint = b2;
    newB.burst = 4;
    modConfData_t candidate = {.pConf = &startup, .reloadRateProfiles = &newA};
    tcpsrv_etry_t endpoints[3] = {0};
    tcpsrv_t servers[3] = {0};
    tcpLstnPortList_t listeners[3] = {0};
    tcpLstnParams_t params[3] = {0};
    const char *names[] = {"A", "A", "B"};
    CHECK(ratelimit_cfgsInit(&startup.ratelimit_cfgs) == RS_RET_OK);
    CHECK(ratelimitAddConfig(&startup, "A", 3600, 2, 7, NULL, 0, NULL, 0, NULL, NULL, 0, 0, 1, 0) == RS_RET_OK);
    CHECK(ratelimitAddConfig(&startup, "B", 0, 3, -1, NULL, 0, NULL, 0, NULL, NULL, 0, 0, 1, 0) == RS_RET_OK);
    for (size_t i = 0; i < 3; ++i) {
        endpoints[i].tcpsrv = &servers[i];
        servers[i].pLstnPorts = &listeners[i];
        listeners[i].cnf_params = &params[i];
        params[i].pszRatelimitName = (uchar *)strdup(names[i]);
        CHECK(params[i].pszRatelimitName != NULL);
        CHECK(ratelimitNewFromConfig(&listeners[i].ratelimiter, &startup, names[i], "unit", NULL) == RS_RET_OK);
    }
    ratelimit_shared_t *originalA = listeners[0].ratelimiter->pShared;
    imtcpPolicyTransaction_t *state = prepareState(&oldConfig, &candidate, endpoints, names);
    CHECK(state != NULL);
    /* The pending map does not retain candidate name/fingerprint storage. */
    nameA[0] = 'Z';
    a1[0] = 'Z';
    CHECK(reloadRateFind(state->preparedPolicies, "A") != NULL &&
          !strcmp(reloadRateFind(state->preparedPolicies, "A")->fingerprint, "A1"));
    nameA[0] = 'A';
    a1[0] = 'A';
    commitState(state);
    failResume = 1;
    CHECK(retireState(state) == RS_RET_RETRY && state->retiredPolicies == NULL && liveLimiters == 4);
    CHECK(listeners[0].ratelimiter->pShared == originalA && listeners[1].ratelimiter->pShared == originalA);
    failResume = 0;
    CHECK(retireState(state) == RS_RET_OK && liveLimiters == 3 && registriesDestroyed == 0);
    oldConfig.reloadRateProfiles = &newA;
    /* An unrelated profile publication rotates metadata but neither resets A
     * nor retains the old generation's map through registry destruction. */
    state = prepareState(&oldConfig, &candidate, endpoints, names);
    CHECK(state != NULL);
    commitState(state);
    CHECK(retireState(state) == RS_RET_OK && liveLimiters == 3 && listeners[0].ratelimiter->pShared == originalA &&
          listeners[1].ratelimiter->pShared == originalA);
    reloadRateProfile_t changedA = newA;
    changedA.fingerprint = a2;
    changedA.burst = 5;
    changedA.severity = 6;
    candidate.reloadRateProfiles = &changedA;
    state = prepareState(&oldConfig, &candidate, endpoints, names);
    CHECK(state != NULL);
    abortState(state);
    CHECK(listeners[0].ratelimiter->pShared == originalA && liveLimiters == 3 && registriesDestroyed == 1);
    state = prepareState(&oldConfig, &candidate, endpoints, names);
    CHECK(state != NULL);
    commitState(state);
    CHECK(listeners[0].ratelimiter->pShared != originalA &&
          listeners[0].ratelimiter->pShared == listeners[1].ratelimiter->pShared);
    CHECK(listeners[0].ratelimiter->pShared->severity == 6 && listeners[0].ratelimiter->pShared->burst == 5);
    CHECK(retireState(state) == RS_RET_OK && liveLimiters == 3);
    oldConfig.reloadRateProfiles = &changedA;
    const char *unbound[] = {NULL, NULL, "B"};
    state = prepareState(&oldConfig, &candidate, endpoints, unbound);
    CHECK(state != NULL);
    commitState(state);
    CHECK(retireState(state) == RS_RET_OK);
    imtcpPolicyOwner_t *unusedA = reloadRateFind(activeRateProfiles, "A")->owner;
    CHECK(unusedA->references == 1);
    /* Rebind unchanged, temporarily unused A: both consumers reuse its retained
     * registry owner, independently of each listener's local budget counters. */
    state = prepareState(&oldConfig, &candidate, endpoints, names);
    CHECK(state != NULL);
    commitState(state);
    CHECK(retireState(state) == RS_RET_OK);
    CHECK(endpoints[0].policyOwner == unusedA && endpoints[1].policyOwner == unusedA);
    state = prepareState(&oldConfig, &candidate, endpoints, names);
    CHECK(state != NULL);
    commitState(state);
    CHECK(retireState(state) == RS_RET_OK);
    /* Remove A only after its last bindings move away. */
    candidate.reloadRateProfiles = &newB;
    state = prepareState(&oldConfig, &candidate, endpoints, unbound);
    CHECK(state != NULL);
    commitState(state);
    CHECK(retireState(state) == RS_RET_OK);
    CHECK(reloadRateFind(activeRateProfiles, "A") == NULL);
    CHECK(prepareState(&oldConfig, &candidate, endpoints, names) == NULL && liveLimiters == 3);
    /* Simulate afterRun invalidating every live listener while a failed-resume
     * transaction still owns the previous private B generation. Published
     * transaction slots must already be detached from runtime storage. */
    oldConfig.reloadRateProfiles = &newB;
    reloadRateProfile_t lastB = newB;
    char b3[] = "B3";
    lastB.fingerprint = b3;
    lastB.burst = 7;
    candidate.reloadRateProfiles = &lastB;
    state = prepareState(&oldConfig, &candidate, endpoints, unbound);
    CHECK(state != NULL);
    commitState(state);
    failResume = 1;
    CHECK(retireState(state) == RS_RET_RETRY && liveLimiters == 4);
    for (size_t i = 0; i < 3; ++i) {
        ratelimitDestruct(listeners[i].ratelimiter);
        free(params[i].pszRatelimitName);
        imtcpPolicyOwnerRelease(&endpoints[i].policyOwner);
        endpoints[i].tcpsrv = NULL;
        listeners[i].cnf_params = NULL;
        CHECK(state->entries[i].runtimeOwner == NULL && state->entries[i].runtimeLimiter == NULL &&
              state->entries[i].runtimeName == NULL);
    }
    reloadRateProfilesDestruct(&activeRateProfiles);
    CHECK(liveLimiters == 1);
    failResume = 0;
    CHECK(retireState(state) == RS_RET_OK);
    CHECK(liveLimiters == 0);
    const int privateDestroyed = registriesDestroyed;
    ratelimit_cfgsDestruct(&startup.ratelimit_cfgs);
    CHECK(registriesDestroyed == privateDestroyed + 1);
    /* Borrowed owner release must not inspect an already-torn-down startup registry. */
    imtcpPolicyOwner_t *borrowed = imtcpPolicyOwnerNew((void *)(uintptr_t)1, NULL);
    CHECK(borrowed != NULL);
    imtcpPolicyOwnerRelease(&borrowed);
    puts("imtcp policy ownership tests passed");
    return 0;
}
