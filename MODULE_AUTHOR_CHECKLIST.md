# Module author checklist (v8)

**State placement**
- [ ] Config/static/shared data in **pData** (per-action).
- [ ] Live handles/buffers in **wrkrInstanceData_t** (per-worker).
- [ ] No sharing of WID across workers.

**Serialization**
- [ ] Inherently serial resources (e.g., a shared stream) guarded by a **mutex in pData**.
- [ ] If any library thread or callback touches shared state, define a **pthread_rwlock_t**
      and document who reads/writes.

**Entry points**
- [ ] `createWrkrInstance`/`freeWrkrInstance` allocate/free WID only.
- [ ] `doAction`/tx callbacks may run concurrently across workers; they **never** share WID.

**Docs in code**
- [ ] Top-of-file “Concurrency & Locking” block explains the above for this module.
- [ ] Doxygen comments on pData/WID typedefs describe lifetime & locking rules.
- [ ] **Documentation**: New parameters added to `doc/source/reference/parameters/` and linked in module `.rst`.

**Testing**
- [ ] Run with `queue.workerThreads > 1`. CI already runs TSAN on the full suite.

**Style**
- [ ] Commit subject ≤ 65 chars; aim for 62. ASCII only. See `CONTRIBUTING.md`.
