/* SPDX-License-Identifier: Apache-2.0 */
/** @file shadow_reload_retirement_test.c
 * @brief Fake-clock/callback oracle for retained retirement retry scheduling.
 * No sleeps, sockets or daemon timing. Exact invocation counts prove that all
 * callers sharing a schedule cannot double-attempt before due; pending state
 * blocks another candidate while resources stay owned through every error.
 * The production rejection selector distinguishes OFF, VALIDATE and ON;
 * OFF after publication/resume failure must not select the ON counter.
 */
#include "config.h"
#include <stdio.h>
#include "rsyslog.h"
#include "../../tools/shadow_reload_retirement.h"

#define CHECK(condition)                                                   \
    do {                                                                   \
        if (!(condition)) {                                                \
            fprintf(stderr, "failed line %d: %s\n", __LINE__, #condition); \
            return 1;                                                      \
        }                                                                  \
    } while (0)

typedef struct fixture_s {
    unsigned calls;
    int owned;
    rsRetVal result;
} fixture_t;

static rsRetVal cleanup(void *const context) {
    fixture_t *const fixture = context;
    ++fixture->calls;
    if (fixture->result == RS_RET_OK) fixture->owned = 0;
    return fixture->result;
}

typedef struct cycle_s {
    unsigned claims, completions, prepared;
    int signalPending, inProgress, gauge, publishedResult;
} cycle_t;

static void claim(void *const context) {
    cycle_t *const cycle = context;
    ++cycle->claims;
    cycle->signalPending = 0;
    cycle->inProgress = cycle->gauge = 1;
}

static void complete(void *const context) {
    cycle_t *const cycle = context;
    ++cycle->completions;
    cycle->inProgress = 0;
    cycle->gauge = cycle->signalPending != 0;
}

static unsigned rejectedValidate, rejectedOn;

static void rejectValidate(void) {
    ++rejectedValidate;
}

static void rejectOn(void) {
    ++rejectedOn;
}

int main(void) {
    shadowReloadRetirementReject(RELOAD_ON_HUP_OFF, rejectValidate, rejectOn);
    CHECK(rejectedValidate == 0 && rejectedOn == 0);
    shadowReloadRetirementReject(RELOAD_ON_HUP_VALIDATE, rejectValidate, rejectOn);
    CHECK(rejectedValidate == 1 && rejectedOn == 0);
    shadowReloadRetirementReject(RELOAD_ON_HUP_ON, rejectValidate, rejectOn);
    CHECK(rejectedValidate == 1 && rejectedOn == 1);
    shadowReloadRetirement_t state = {0};
    fixture_t fixture = {.owned = 1, .result = RS_RET_ERR};
    uint64_t now = 1000000;
    unsigned newCandidates = 0;

    CHECK(shadowReloadRetirementTry(&state, now, 1, 0, cleanup, &fixture) == RS_RET_ERR);
    CHECK(state.pending && state.lastError == RS_RET_ERR && state.retryCount == 0);
    CHECK(state.delayUsec == 100000 && state.nextUsec == now + 100000);
    CHECK(fixture.calls == 1 && fixture.owned);
    /* Main-loop, request-begin and end-of-process cleanup share this guard. */
    for (unsigned entry = 0; entry < 3; ++entry)
        CHECK(shadowReloadRetirementTry(&state, state.nextUsec - 1, 1, 0, cleanup, &fixture) == RS_RET_RETRY);
    CHECK(fixture.calls == 1 && state.lastError == RS_RET_ERR && state.retryCount == 0);
    if (!state.pending) ++newCandidates;
    CHECK(newCandidates == 0 && fixture.owned);

    /* Exercise the production claim/completion branches, not a copied gate.
     * A blocked HUP is accounted and its legacy hooks run once; a newer signal
     * arriving during those hooks survives. Published status is untouched. */
    cycle_t cycle = {.signalPending = 1, .publishedResult = 42};
    int blocked = 0;
    if (!shadowReloadRetirementClaim(&state, &blocked, claim, &cycle)) ++cycle.prepared;
    CHECK(blocked && cycle.claims == 1 && cycle.inProgress && cycle.prepared == 0);
    cycle.signalPending = 1; /* New HUP during the one actual legacy hook cycle. */
    CHECK(shadowReloadRetirementComplete(&blocked, complete, &cycle));
    CHECK(!blocked && cycle.completions == 1 && !cycle.inProgress && cycle.gauge);
    CHECK(cycle.signalPending && cycle.publishedResult == 42 && state.pending);
    CHECK(!shadowReloadRetirementComplete(&blocked, complete, &cycle));
    CHECK(cycle.completions == 1 && cycle.prepared == 0);

    now = state.nextUsec;
    CHECK(shadowReloadRetirementTry(&state, now, 1, 0, cleanup, &fixture) == RS_RET_ERR);
    CHECK(state.retryCount == 1 && state.delayUsec == 200000 && fixture.calls == 2);
    for (unsigned retry = 0; retry < 8; ++retry) {
        now = state.nextUsec;
        const uint64_t before = state.delayUsec;
        CHECK(shadowReloadRetirementTry(&state, now, 1, 0, cleanup, &fixture) == RS_RET_ERR);
        CHECK(state.delayUsec == (before >= 2500000 ? 5000000 : before * 2));
        CHECK(state.nextUsec == now + state.delayUsec && fixture.owned);
    }
    CHECK(state.delayUsec == 5000000 && state.retryCount == 9);
    fixture.result = RS_RET_RETRY;
    CHECK(shadowReloadRetirementTry(&state, state.nextUsec, 1, 0, cleanup, &fixture) == RS_RET_RETRY);
    CHECK(state.pending && state.lastError == RS_RET_RETRY && state.retryCount == 10 && fixture.owned);
    const unsigned calls = fixture.calls;
    CHECK(shadowReloadRetirementTry(&state, UINT64_MAX, 0, 0, cleanup, &fixture) == RS_RET_RETRY);
    CHECK(fixture.calls == calls && state.retryCount == 10);
    CHECK(shadowReloadRetirementTry(&state, state.nextUsec - 1, 1, 0, cleanup, &fixture) == RS_RET_RETRY);
    CHECK(fixture.calls == calls);
    fixture.result = RS_RET_OK;
    CHECK(shadowReloadRetirementTry(&state, state.nextUsec, 1, 0, cleanup, &fixture) == RS_RET_OK);
    CHECK(!state.pending && state.lastError == RS_RET_OK && state.retryCount == 11 && !fixture.owned);
    if (!state.pending) ++newCandidates;
    CHECK(newCandidates == 1);
    CHECK(!shadowReloadRetirementClaim(&state, &blocked, claim, &cycle));
    CHECK(!blocked && cycle.claims == 2);
    CHECK(!shadowReloadRetirementComplete(&blocked, complete, &cycle));

    /* An earlier explicit resume failure schedules the same deferred cleanup:
     * end-of-process must not call it immediately a second time. */
    fixture.owned = 1;
    fixture.result = RS_RET_ERR;
    shadowReloadRetirementFailed(&state, now, fixture.result);
    CHECK(state.retryCount == 0 && state.delayUsec == 100000);
    CHECK(shadowReloadRetirementTry(&state, now, 1, 0, cleanup, &fixture) == RS_RET_RETRY);
    CHECK(fixture.calls == calls + 1 && fixture.owned);
    /* Only final post-join teardown may bypass deadline/clock failure once. */
    fixture.result = RS_RET_OK;
    CHECK(shadowReloadRetirementTry(&state, now, 0, 1, cleanup, &fixture) == RS_RET_OK);
    CHECK(!state.pending && !fixture.owned && state.retryCount == 1);

    /* If the clock fails on the first error, a zero-based deadline is only
     * an anchor: no subsequent callback runs until monotonic time recovers. */
    fixture.owned = 1;
    fixture.result = RS_RET_ERR;
    CHECK(shadowReloadRetirementTry(&state, 0, 0, 0, cleanup, &fixture) == RS_RET_ERR);
    CHECK(state.nextUsec == 100000 && state.retryCount == 0);
    const unsigned unavailableCalls = fixture.calls;
    CHECK(shadowReloadRetirementTry(&state, 0, 0, 0, cleanup, &fixture) == RS_RET_RETRY);
    CHECK(fixture.calls == unavailableCalls && fixture.owned);
    fixture.result = RS_RET_OK;
    CHECK(shadowReloadRetirementTry(&state, 200000, 1, 0, cleanup, &fixture) == RS_RET_OK);
    CHECK(!state.pending && !fixture.owned && state.retryCount == 1);

    /* Deadline saturation and unavailable monotonic clocks cannot wrap/spin. */
    fixture.owned = 1;
    fixture.result = RS_RET_ERR;
    CHECK(shadowReloadRetirementTry(&state, UINT64_MAX - 1, 1, 0, cleanup, &fixture) == RS_RET_ERR);
    CHECK(state.nextUsec == UINT64_MAX);
    CHECK(shadowReloadRetirementTry(&state, UINT64_MAX - 1, 1, 0, cleanup, &fixture) == RS_RET_RETRY);
    CHECK(shadowReloadRetirementTry(&state, 0, 0, 0, cleanup, &fixture) == RS_RET_RETRY);
    fixture.result = RS_RET_OK;
    CHECK(shadowReloadRetirementTry(&state, UINT64_MAX, 1, 0, cleanup, &fixture) == RS_RET_OK);
    CHECK(!state.pending && !fixture.owned);
    return 0;
}
