# Unit Tests

This directory contains source files and notes for fast C unit tests covering
small, isolated rsyslog-owned helpers.

## Scope

- `tests/` is the single Automake-owned test subtree for the project.
- `tests/unit/` organizes in-process unit-test sources that do not require
  daemon startup, `diag.sh`, module loading, or the wider testbench runtime.
- Unit tests should target helpers that can be exercised directly with explicit
  inputs and local state.
- Keep source organization here, but do not add a separate `tests/unit`
  Automake harness. In recursive Automake trees, a second test-owning subdir
  makes top-level `make check TESTS=...` selection brittle.

## Build and run

From the repository root:

```sh
make check
```

This runs the unit tests together with the rest of the Automake test suite.

To run one unit test binary through the shared harness from the repository
root:

```sh
make check TESTS='runtime_unit_linkedlist'
```

## Conventions

- Prefer one test binary per production helper or component.
- Keep tests self-contained and deterministic.
- Do not depend on `tests/diag.sh` or the shell testbench helpers here.
- Keep `tests/` as the only recursive test-owning subtree. Organize sources
  under `tests/unit/` rather than introducing a second Automake test harness.
- Avoid broad runtime stubbing. If a candidate needs substantial global runtime
  setup, it probably belongs in integration coverage instead of `tests/unit/`.
