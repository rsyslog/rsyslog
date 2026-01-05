# AGENTS.md – Runtime subtree

These instructions apply to all files under `runtime/`, which implement the
rsyslog core (message queues, networking backends, parsers, scheduler, stats
collection, and process orchestration).

## Purpose & scope
- Treat this tree as the authoritative implementation of the rsyslog engine.
  Changes here affect every module and deployment profile.
- Consult the top-level `AGENTS.md` and component-specific guides (e.g.
  `plugins/AGENTS.md`) when a change crosses directory boundaries.

## Key components
- `rsyslog.c` / `rsyslog.h`: main entry point and daemon lifecycle helpers.
- `modules.c`, `module-template.h`: module loader contracts shared with
  `plugins/` and `contrib/`.
- `queue.c`, `wti.c`, `wtp.c`: work queue implementation and worker threads.
- `tcpsrv.c`, `tcpclt.c`, `net*.c`: TCP/TLS listeners and clients.
- `parser.c`, `prop.c`, `template.c`: core message parsing and property engine.
- `statsobj.c`, `dynstats*.c`: statistics registry and dynamic counters.
- `timezones.c`, `datetime.c`: time conversion helpers.

## Build & validation
- **Efficient Build:** Use `make -j$(nproc) check TESTS=""` to incrementally build the core, shared libraries, and test dependencies. This ensures that tests can dynamically load the runtime via `-M../runtime/.libs`.
- **Bootstrap/Configure:** Run `./autogen.sh` and `./configure --enable-testbench` only when build scripts change (`configure.ac`, `Makefile.am`, etc.) or if the `Makefile` is missing. Run these from the repository root.
- **Run Tests:** Prefer targeted test runs:
  - Directly invoke the most relevant shell test under `tests/` (e.g. `./tests/queue-persist.sh`).
  - Use `make check TESTS='<script>.sh'` when you need automake’s harness or Valgrind variants (`*-vg.sh`).
  - For configuration validation edits, run `./tests/validation-run.sh`.
- When changing exported symbols, update `runtime/Makefile.am` and ensure the library version script (if touched) remains consistent with existing SONAME policies.

## Coding expectations
- Follow `COMMENTING_STYLE.md` and add/update "Concurrency & Locking" blocks in
  files that manage shared state (queues, network sessions, statistics).
- Keep worker/thread behavior aligned with `MODULE_AUTHOR_CHECKLIST.md` rules:
  per-worker state stays in `wrkrInstanceData_t`, shared state is guarded in
  per-action data structures.
- Update `doc/ai/module_map.yaml` when locking guarantees, queue semantics, or
  exported helper APIs change so module authors know what to depend on.
- Update or create unit helpers (e.g. under `tests/` or `runtime/hashtable/`)
  when you modify reusable primitives.

## Coordination & documentation
- Notify module owners (via metadata or review notes) when adjusting
  `module-template.h`, initialization flows, or statistics surfaces consumed by
  plugins.
- Cross-reference user docs in `doc/` if behavior visible to operators changes
  (configuration syntax, stats counters, TLS requirements, etc.).
- Leave `ChangeLog` and `NEWS` edits to the maintainers; do not modify those
  files in routine patches.

## Debugging tips
- Enable extra runtime logging with `export RSYSLOG_DEBUG="..."` in tests (see
  `tests/diag.sh`) when chasing race conditions.
- Use the testbench Valgrind helpers by running the corresponding `*-vg.sh`
  scripts to flush out memory and threading regressions.
