/* SPDX-License-Identifier: Apache-2.0 */
/** @file shadow_reload_retirement.h
 * @brief Main-thread-only retry scheduling; no resource ownership transfers.
 */
#ifndef INCLUDED_SHADOW_RELOAD_RETIREMENT_H
#define INCLUDED_SHADOW_RELOAD_RETIREMENT_H
#include <stdint.h>
#include "rsyslog.h"

/** @brief One retained retirement episode, mutated only by the control thread. */
typedef struct shadowReloadRetirement_s {
    int pending; /**< Blocks new candidates until cleanup succeeds. */
    rsRetVal lastError; /**< Last actual callback error, not a deferred poll result. */
    uint64_t retryCount; /**< Actual retry cleanup attempts, excluding the initial failure. */
    uint64_t delayUsec; /**< Current exponential delay, capped at five seconds. */
    uint64_t nextUsec; /**< Earliest retry on the monotonic clock. */
} shadowReloadRetirement_t;

/** @brief Record an actual failed resume/cleanup without discarding owned state.
 * @param state Main-thread-owned scheduler. @param now Monotonic microseconds.
 * @param error Actual callback error. No allocation, locks or ownership transfer.
 * A new episode resets retryCount; a successful episode retains its final count.
 */
static void shadowReloadRetirementFailed(shadowReloadRetirement_t *const state,
                                         const uint64_t now,
                                         const rsRetVal error) {
    if (!state->pending) {
        state->retryCount = 0;
        state->delayUsec = 100000;
    } else {
        state->delayUsec = state->delayUsec >= 2500000 ? 5000000 : state->delayUsec * 2;
    }
    state->pending = 1;
    state->lastError = error;
    state->nextUsec = now > UINT64_MAX - state->delayUsec ? UINT64_MAX : now + state->delayUsec;
}

/** @brief Attempt cleanup only when due, or once at final post-join shutdown.
 * @param state Main-thread-owned schedule. @param now Monotonic microseconds.
 * @param clockValid False defers pending work until monotonic time recovers.
 * @param finalShutdown One-time bypass after input joins; never a normal retry.
 * @param cleanup Synchronous callback retaining all owned resources on failure.
 * @param context Borrowed callback context. No callback runs for a deferred poll.
 * @return Actual callback result, or RS_RET_RETRY when not due/clock unavailable.
 * One cleanup attempt may resume then retire. Callback RS_RET_RETRY is a real
 * failed attempt and receives the same backoff.
 * A clock failure uses zero for a new failure's deadline and defers subsequent
 * retries while time is unavailable: no wall-clock fallback and no busy loop.
 */
static rsRetVal shadowReloadRetirementTry(shadowReloadRetirement_t *const state,
                                          const uint64_t now,
                                          const int clockValid,
                                          const int finalShutdown,
                                          rsRetVal (*cleanup)(void *),
                                          void *const context) {
    if (state->pending && !finalShutdown && (!clockValid || now < state->nextUsec)) return RS_RET_RETRY;
    if (state->pending && state->retryCount != UINT64_MAX) ++state->retryCount;
    const rsRetVal ret = cleanup(context);
    if (ret != RS_RET_OK) {
        shadowReloadRetirementFailed(state, clockValid ? now : 0, ret);
    } else {
        state->pending = 0;
        state->lastError = RS_RET_OK;
        state->delayUsec = 0;
        state->nextUsec = 0;
    }
    return ret;
}
/** @brief Claim/account a HUP once, even when retirement blocks its candidate.
 * @param state Borrowed main-thread schedule. @param blocked Owned cycle flag.
 * @param account Synchronous accounting/claim callback. @param context Borrowed context.
 * @return Nonzero skips candidate preparation; legacy hooks must still run once.
 */
static int shadowReloadRetirementClaim(const shadowReloadRetirement_t *const state,
                                       int *const blocked,
                                       void (*account)(void *),
                                       void *const context) {
    account(context);
    *blocked = state->pending;
    return *blocked;
}

/** @brief Complete exactly one explicitly rejected blocked cycle.
 * @param blocked Main-thread cycle flag, cleared before callback.
 * @param complete Synchronous rejection accounting preserving published status.
 * @param context Borrowed callback context; newer signal state must remain intact.
 * @return Nonzero means no candidate processing or cleanup is permitted this cycle.
 */
static int shadowReloadRetirementComplete(int *const blocked, void (*complete)(void *), void *const context) {
    if (!*blocked) return 0;
    *blocked = 0;
    complete(context);
    return 1;
}
#endif
