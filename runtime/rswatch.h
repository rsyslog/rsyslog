#ifndef INCLUDED_RSWATCH_H
#define INCLUDED_RSWATCH_H

/*
 * rswatch is the runtime-internal watched-file scheduler used by subsystems
 * that can reload a single configuration file without doing a full HUP sweep.
 *
 * Algorithm overview
 * - Each registration describes one concrete file path plus a debounce window
 *   and a callback.
 * - On inotify-capable builds, rswatch watches the file's parent directory and
 *   matches events by basename. This deliberately handles the common
 *   write-temp-and-rename save pattern, where the file inode changes but the
 *   configured path stays the same.
 * - I/O readiness is integrated into rsyslogd's main housekeeping loop via
 *   rswatchGetWaitFd(), rswatchComputeTimeoutMs(), rswatchProcessIo(), and
 *   rswatchDispatchDue(). rswatch itself does not create a worker thread.
 * - When rswatchProcessIo() sees an event for a watched basename, it marks the
 *   registration pending and stores due_at = now + debounce_ms.
 * - rswatchDispatchDue() later pops registrations whose debounce deadline has
 *   expired and invokes their callback.
 *
 * Concurrency and locking
 * - The subsystem has one process-global state object guarded by one mutex.
 * - The mutex protects the registration list, the backend fd, backend-disable
 *   state, and per-registration pending/due bookkeeping.
 * - Callbacks are never executed while the rswatch mutex is held. This keeps
 *   reload code free to take its own locks and avoids turning rswatch into a
 *   lock-ordering bottleneck in the main loop.
 * - Registrations and unregistrations are expected on configuration/control
 *   paths, not on message-processing hot paths.
 *
 * Failure model
 * - On builds without inotify support, registration returns
 *   RS_RET_NOT_IMPLEMENTED so callers can degrade to HUP-only behavior.
 * - If the backend fails after registrations exist, rswatch disables itself,
 *   clears pending state, and leaves already-registered users in place so the
 *   rest of rsyslog continues to operate without watched reloads.
 */

#include <stdint.h>
#include "rsyslog.h"

typedef struct rswatch_handle_s rswatch_handle_t;

typedef void (*rswatch_cb_t)(void *ctx, const char *trigger);

typedef struct rswatch_desc_s {
    const char *id;
    const char *path;
    unsigned int debounce_ms;
    rswatch_cb_t cb;
    void *ctx;
} rswatch_desc_t;

rsRetVal rswatchRegister(const rswatch_desc_t *desc, rswatch_handle_t **out);
void rswatchUnregister(rswatch_handle_t *handle);
int rswatchGetWaitFd(void);
int rswatchComputeTimeoutMs(uint64_t now_ms, int default_timeout_ms);
void rswatchProcessIo(uint64_t now_ms);
void rswatchDispatchDue(uint64_t now_ms);
void rswatchExit(void);

#endif /* INCLUDED_RSWATCH_H */
