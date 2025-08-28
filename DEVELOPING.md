# Developing rsyslog (v8 engine essentials)

This short guide is the **canonical entry point** for humans and AI coding agents.
It captures the v8 worker model and locking rules that module code must follow.

## v8 worker model (normative)
* The framework may run **multiple workers per action**.
* `wrkrInstanceData_t` (WID) is **per-worker, thread-local** → **no locks needed** inside WID.
* `pData` (per-action/shared) may be touched by multiple workers and/or extra threads → **guard mutable/shared members**.
* **Inherently serial resources** (e.g., a FILE/strm shared in pData) **must be serialized** inside the module using a mutex in pData.
* **Direct queues** do **not** remove the need to serialize serial resources.

### Where state goes
| Kind of state                         | Put it in           | Locking                                            |
|--------------------------------------|---------------------|----------------------------------------------------|
| Config, shared counters, streams     | `pData` (per-action)| Guard if mutated or not thread-safe                |
| Live handles, sockets, buffers       | `wrkrInstanceData_t`| No locks (WID is owned by exactly one worker)      |
| Library thread + callbacks (I/O)     | `pData` + rwlock    | Define who reads/writes; document in comments      |

### Locking rules (quick)
1. Never share a WID across workers.
2. Any shared mutable `pData` member → protect with `pthread_mutex_t`
   (or `pthread_rwlock_t` if a background thread is involved).
3. If a module uses an external/library thread, document **who takes read/write**.
4. Transactions follow the same rules as `doAction`.
5. Test with workerThreads > 1.

## Canonical examples (start here)
* `plugins/omfile/*` → inherently serial: uses a pData mutex to guard write/flush.
* `plugins/omfwd/*`  → per-worker sockets in WID, typically no additional locks.
* `plugins/omelasticsearch/*` → per-worker curl & batch in WID.
* `plugins/omazureeventhubs/*` → two-thread model (rsyslog worker + Proton): needs an rwlock across threads (fix in progress).
* `contrib/omhttp/*` → per-worker curl & buffers in WID.
* `contrib/mmkubernetes/*` → WID is per-thread; shared caches live in pData and
  must be guarded (see code).

## Entry points (what modules implement)
* `createWrkrInstance` / `freeWrkrInstance` — allocate/free WID (thread-local).
* `doAction` — may run concurrently across workers of the same action.
* `beginTransaction` / `commitTransaction` / `rollbackTransaction` — same concurrency rules as `doAction`.

> **Rule of thumb:** if multiple workers could touch it, it lives in `pData` and must be protected.

## Notes on current code
* `ommysql`: a global RW-lock was introduced historically; it will be refactored soon.
  Consider it a historical note, not a pattern to copy.
* `omazureeventhubs`: ensure all Proton callbacks take the read lock and worker paths
  that mutate handles take the write lock. A follow-up patch will address this.

## Coding style
* US-ASCII, subject ≤ 65 chars (aim 62), see `COMMENTING_STYLE.md` and `CONTRIBUTING.md`.

## Safe starter tasks for agents
* Add a **“Concurrency & Locking”** comment block at the top of output modules.
* Ensure inherently serial modules guard pData streams with a mutex.
* Verify modules with external threads hold the documented read/write locks on **all** callback paths (TSAN already runs in CI).

## Pointers
* Developer docs: `doc/` (Sphinx).
* Tests: `tests/`.
* Module sources: `plugins/` and `contrib/`.
* AI module map: `doc/ai/module_map.yaml` (per-module paths & locking hints).

---
_This file is intentionally brief and normative. Longer background belongs in Sphinx._
