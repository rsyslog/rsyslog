---
name: rsyslog_local_container_testing
description: Mirror rsyslog run_checks.yml container validation locally, including Cubic review where applicable, the clang static analyzer job, the change-gated Ubuntu 26.04 run-ci.sh check run, service-skip validation, clean-tree rules, and container path caveats.
---

# rsyslog_local_container_testing

Use this skill when validating rsyslog implementation changes through local dev
containers, and treat it as the final pre-finish gate when Docker, Podman, or
equivalent container tooling is available. Local container validation should
mirror the relevant GitHub Actions container jobs, including their relevance
gates, instead of inventing a parallel local-only flow. If container tooling is
unavailable, report that limitation explicitly rather than silently
substituting a weaker gate.

## When To Use

Use this for PR-ready validation whenever local container support is available.
It is often faster than waiting for GitHub CI to discover environment-specific
failures, especially static-analyzer findings, compiler/toolchain differences,
dependency differences, generated build state issues, and service-test skip
behavior.

Focused host-side tests from `rsyslog_test` are still the right first step when
debugging a specific behavior. After the patch is stable, run this skill before
pushing if the change is intended for review.

## Pre-PR Testbed Tiers

Use these tiers as a session testbed before opening or updating PRs. The goal is
to catch the failures that otherwise cost a full GitHub Actions turnaround and
follow-up babysitting cycle, without cloning the entire CI matrix for every
small patch.

Start with the repository helper:

```sh
devtools/local-validation-plan.sh
```

The default mode classifies the current local diff and prints the recommended
validation path. It includes committed branch changes, staged and unstaged
tracked edits, and untracked files. This is important because PR work is often
validated before everything is committed.

When you want deterministic execution instead of a checklist, use:

```sh
devtools/local-validation-plan.sh --run
```

`--run` executes the selected path and exits on the first required failure.
Optional local linters still remain tool-presence guarded, so missing optional
developer tools do not turn a documentation or tooling-only change into a
container validation failure.

Before starting any heavy local validation, run the relevant cheap local checks
that are available in the environment. These checks should be diff-scoped and
tool-presence guarded:

- changed shell scripts: `shellcheck`
- changed scripts with a POSIX `sh` shebang: `checkbashisms -p`; on
  Debian/Ubuntu install it with `sudo apt-get install -y devscripts`. Do not
  run it as a full-testbench gate by default, because many tests intentionally
  use Bash-oriented testbench conventions. Keep this check diff-scoped so
  runtime stays proportional to the changed files.
- changed Python files: `devtools/format-python.sh --check-if-available`
- changed GitHub Actions workflows: `actionlint` and pinned `zizmor`
- changed Dockerfiles: `hadolint`
- changed infrastructure/config files: `trivy config` on the changed paths or
  smallest relevant directory
- larger source/test PRs: `jscpd` as an advisory duplication check
- changed C sources or headers MUST run the exact read-only check:
  `devtools/format-code.sh --git-changed --check --check-if-available`. This
  is a read-only `clang-format-18` dry-run gate for deterministic validation;
  if the exact formatter is unavailable, it warns and leaves hosted CI or a
  fuller local environment to cover the gap. CI will not pass with improperly
  formatted C/H code. Use `devtools/format-code.sh --git-changed` separately
  when you intentionally want to rewrite local files.

Do not run `cppcheck` routinely unless a maintainer explicitly asks for it; it
is too noisy for routine rsyslog PR validation.

- **Agent-documentation and skill-only changes**. If the diff is limited to
  `AGENTS.md`, `AGENTS.local.md`, `.agent/skills/**`, or `.codex/skills/**`,
  do not run the Ubuntu 26.04 container check or static analyzer merely because
  validation instructions changed. Validate these edits with normal text review,
  Markdown/link sanity checks where available, and a small command dry-run only
  when the edited instructions include command snippets whose syntax is
  uncertain. If the agent-doc change also edits code, testbench files,
  workflows, build files, or scripts, validate those touched areas normally.
- **User-facing documentation changes**. If the diff changes rendered
  user-facing docs, such as `doc/source/**` or Sphinx support files that affect
  that tree, do not run the Ubuntu 26.04 runtime check just because docs
  changed. Instead run a high-concurrency docs build:
  `./doc/tools/build-doc-linux.sh --clean --format html --jobs "${RSYSLOG_LOCAL_DOC_JOBS:-$(nproc)}"`.
  Use `--strict` for larger, structural, or navigation-heavy documentation
  edits. Internal docs that are not rendered into the user manual, such as
  `doc/ai/**`, repository agent guides, and skill files, do not require this
  docs build unless they also change rendered Sphinx inputs.
- **Tier 1: default PR-ready gate for code or testbench changes**. Run the
  local Cubic review when applicable, the existing static-analyzer pass, and a
  change-gated Ubuntu 26.04 `devtools/run-ci.sh` check. This is the normal
  local confidence gate for C, parser, module, runtime, `tests/*.sh`,
  `diag.sh`, `Makefile.am`, `configure.ac`, and `run_checks.yml` changes.
- **Tier 2: compile portability gate for non-trivial C/header changes**. Add
  the two build-only compile lanes from `run_checks.yml`:
  `clang21-ndebug` and `gcc15-gnu23-debug`. Use this when a change touches
  shared headers, compiler-sensitive code, generated C paths, configure/build
  inputs, or anything that could behave differently under debug/no-debug or
  GNU23 compilation.
- **Tier 3: risk-triggered specialist lanes**. Add only the lanes that match the
  touched area or risk profile:
  `ubuntu_26_san` for memory, parser, string, JSON, property, and bounds work;
  `ubuntu_26_tsan` for threading, queue, action lifecycle, or lock-order work;
  `ubuntu_26_imtcp_no_epoll` for imtcp and network input changes;
  `ubuntu_22_distcheck` or `kafka_distcheck_CI` for dist, autotools,
  packaging, or Kafka Makefile changes.

Keep macOS, full distro matrix, package builds, and service-backed matrix jobs
as CI-owned by default. Run them locally only when the change directly targets
that area and the local host can reproduce the required environment.

Keep i386 and Ubuntu 24 regular runtime coverage cloud-only by default. Run
them locally only when reproducing a lane-specific failure, validating their
workflow/container, or when a maintainer explicitly asks for that local lane.

Normal PR-ready local validation is change-gated. It must not force the full
test suite by default. Force full coverage only for daily, weekly, release,
flake-campaign, maintainer-requested, or relevance-gate validation work.

## Local Capacity Knobs

Use the local machine's configured capacity for broad local checks instead of
hard-coding this host's current limit into instructions. Unknown machines should
default conservatively:

```sh
: "${RSYSLOG_LOCAL_CHECK_JOBS:=10}"
: "${RSYSLOG_LOCAL_BUILD_JOBS:=10}"
export CI_MAKE_OPT="-j${RSYSLOG_LOCAL_BUILD_JOBS}"
export CI_MAKE_CHECK_OPT="-j${RSYSLOG_LOCAL_CHECK_JOBS}"
```

`RSYSLOG_LOCAL_CHECK_JOBS` is the normal local `make check` concurrency gauge.
It should be set by the user or machine profile, for example in `.bashrc`.
Deflake and overload experiments are prompt-driven and must use explicit
one-off `-jN` values instead of changing this normal validation knob.

## Cubic Review

Run the local `cubic` CLI as an AI review gate when it applies. Cubic is a small
local shim that forwards the review request to the configured cloud AI service,
so it can normally run in parallel with the local static analyzer.

- **Docs-only changes**: do not run Cubic.
- **Code changes (`*.c`, `*.h`, grammar/parser/runtime/module logic)**: always
  run Cubic when the command is installed and reachable.
- **Test-only changes**: run Cubic for non-trivial test logic, timing/sync
  changes, helper changes, or broad testbench behavior.
- **Workflow, build, and tooling changes**: run Cubic when the change is
  non-trivial, behavior-affecting, security-sensitive, or large. Use
  `actionlint`, pinned `zizmor`, and the relevant local linters as the primary
  deterministic checks for workflow syntax/security.
- **Mixed or large changes**: run Cubic.

Hosted Cubic or Gemini PR comments are useful additional feedback, but they do
not replace local Cubic for code changes and do not replace local analyzer,
build, or container checks.

## Extended Lane Triggers

Use extended lanes to target a plausible failure mode that the default
change-gated Ubuntu 26.04 check would not expose. Do not add lanes by habit.

- **Compile portability lanes (`clang21-ndebug`, `gcc15-gnu23-debug`)**:
  run for shared header changes, configure/build/autotools changes,
  compiler-sensitive C changes, macro/inline helper changes, generated C paths,
  debug/no-debug sensitive code, GNU23/C standard sensitivity, or broad C
  changes where one normal compiler is weak evidence.
- **SAN (`ubuntu_26_san`)**: run for parser, string, buffer, JSON, property,
  length accounting, allocation, ownership, malformed-input, crash,
  undefined-behavior, or security-adjacent changes.
- **TSAN (`ubuntu_26_tsan`)**: run for queues, actions, worker lifecycle,
  shutdown, retry, timers, locks, atomics, condition variables, shared state,
  readiness/polling/synchronization test changes, and all changes to
  `runtime/wti.*` or `runtime/wtp.*`.
- **imtcp poll/select (`ubuntu_26_imtcp_no_epoll`)**: run when touching imtcp
  behavior that could differ between epoll and poll/select mode.
- **Distribution checks (`ubuntu_22_distcheck`, mock distcheck, or
  `kafka_distcheck_CI`)**: run when adding, renaming, or deleting files/tests,
  changing `Makefile.am`, `configure.ac`, `m4`, packaging lists, generated
  artifacts, or Kafka build/test plumbing.
- **Service-specific lanes**: run only when directly relevant to the touched
  module, tests, helpers, build plumbing, or relevance logic. This includes
  Kafka, Elasticsearch, VictoriaLogs/omhttp, MySQL/libdbi, imfile, and similar
  focused families. For imjournal, local runtime tests are normally not
  representative unless the host has a usable journal environment; document the
  limitation and rely on CI/static checks when local runtime coverage is not
  practical.
- **i386 and Ubuntu 24**: leave cloud-only by default. Use local runs only for
  lane-specific reproduction, explicit 32-bit ABI/width work, dependency or
  Ubuntu-LTS package behavior, or workflow/container changes in those lanes.

## Preferred Order

For the PR-ready final gate, run Cubic where applicable and two container
validations for broad local confidence. Mirror these
`.github/workflows/run_checks.yml` paths:

1. Local Cubic review and the `clang static analyzer` job's container command.
   These can run in parallel when the agent harness supports parallel commands.
2. The change-gated Ubuntu 26.04 `devtools/run-ci.sh` check run, only after the
   analyzer and Cubic results are clean or their findings are understood.

This sequence is what "PR-ready local container validation" means in agent
status reports. It is intentionally not an unconditional full-suite run; it
must use the same conservative change-gating and service-relevance model as
regular PR CI. A focused `devtools/run-ci.sh TEST=...` or build-only
`TESTS=""` container command is useful supplemental evidence, but it is only a
targeted container test unless this skill explicitly calls that reduced lane
acceptable for the touched area. Hosted CI, including hosted Cubic/Gemini
comments, does not replace this local container gate.

Keep runtimes and summarize failures separately for each run.

Use the skill's configured dev image or another explicit CI-equivalent image.
The Docker Hub `rsyslog/rsyslog_dev_base_ubuntu:26.04` image is acceptable for
normal local dev-container validation. When the task specifically validates a
locally built runtime or dev image, use that locally built image/tag for the
container-under-test and record its image ID.

For normal local validation, do not set `RSYSLOG_CONTAINER_UID=''`. Leaving
`RSYSLOG_CONTAINER_UID` unset lets `devtools/devcontainer.sh` run the container
process as the host uid/gid and inject a matching passwd/group entry when
needed. This prevents generated build products from being owned by the dev
image's default user, which is often a different uid than the host checkout
owner. Set `RSYSLOG_CONTAINER_UID=''` only when intentionally reproducing the
exact GitHub Actions default-container-user behavior, and expect to normalize
ownership afterwards.

## Clean Tree Rule

Before switching compiler, sanitizer flags, configure options, container image,
or test mode, clean generated build state:

```sh
make distclean || true
rm -f tests/runtime_unit_gss_token_util \
  tests/runtime_unit_linkedlist \
  tests/runtime_unit_stringbuf
```

Skip this only for an immediate rerun with unchanged settings.

If a previous container run left generated files owned by root or by an
unexpected container UID, `make distclean` may print permission errors or the
next `autoreconf` may fail while opening `autom4te.cache`. In that case,
normalize ownership and writable bits in the throwaway worktree before cleaning
again:

```sh
sudo chown -R "$(id -u):$(id -g)" .
chmod -R u+rwX,go+rwX .
make distclean || true
```

Use this only for local validation worktrees where the generated build output is
not part of the intended diff. Re-check `git status --short` afterwards and
remove only untracked build products or logs, not source changes.

Container configure runs leave generated Makefiles with absolute paths from
inside the container, such as `/rsyslog/missing`. Reconfigure on the host before
running host-side `make` in the same tree.

## Run 1: run_checks.yml Clang Static Analyzer

Clean before analyzer mode so `scan-build` recompiles the tree:

```sh
make distclean || true
rm -rf scan-build-report clang-analyzer.log
```

Run:

```sh
RSYSLOG_DEV_CONTAINER='rsyslog/rsyslog_dev_base_ubuntu:26.04' \
SCAN_BUILD='scan-build' \
SCAN_BUILD_CC='clang' \
SCAN_BUILD_REPORT_DIR='scan-build-report' \
CI_MAKE_OPT='-j20' \
DOCKER_RUN_EXTRA_OPTS='-e SCAN_BUILD -e SCAN_BUILD_CC -e SCAN_BUILD_REPORT_DIR' \
RSYSLOG_CONFIGURE_OPTIONS_EXTRA='--disable-elasticsearch --disable-elasticsearch-tests --disable-imkafka --disable-omkafka --disable-kafka-tests --disable-mysql --disable-mysql-tests' \
devtools/devcontainer.sh --rm devtools/run-static-analyzer.sh 2>&1 | tee clang-analyzer.log
```

The analyzer job intentionally has its own concurrency knob. In `run_checks.yml`
it uses `CI_MAKE_OPT='-j20'`; do not infer this from the broad compile matrix.

## Optional Tier 2: run_checks.yml Compile Matrix

Run these before PR for non-trivial C/header or portability-sensitive changes.
They are build-only checks and normally finish faster than a failed PR cycle.
Clean generated state before switching between configurations.

For `clang21-ndebug`:

```sh
make distclean || true
/usr/bin/time -p env \
RSYSLOG_DEV_CONTAINER='rsyslog/rsyslog_dev_base_ubuntu:26.04' \
CI_CONFIGURE_CACHE='1' \
CC='clang-21' \
CFLAGS='-g' \
RSYSLOG_CONFIGURE_OPTIONS_EXTRA='--enable-debug=no' \
devtools/devcontainer.sh --rm devtools/run-configure.sh
/usr/bin/time -p env \
RSYSLOG_DEV_CONTAINER='rsyslog/rsyslog_dev_base_ubuntu:26.04' \
devtools/devcontainer.sh --rm make -j20
```

For `gcc15-gnu23-debug`:

```sh
make distclean || true
/usr/bin/time -p env \
RSYSLOG_DEV_CONTAINER='rsyslog/rsyslog_dev_base_ubuntu:26.04' \
CI_CONFIGURE_CACHE='1' \
CC='gcc-15' \
CFLAGS='-g -std=gnu23' \
RSYSLOG_CONFIGURE_OPTIONS_EXTRA='--enable-debug=yes --disable-omamqp1' \
devtools/devcontainer.sh --rm devtools/run-configure.sh
/usr/bin/time -p env \
RSYSLOG_DEV_CONTAINER='rsyslog/rsyslog_dev_base_ubuntu:26.04' \
devtools/devcontainer.sh --rm make -j20
```

## Run 2: run_checks.yml Ubuntu 26.04 Check

Use the same `devtools/run-ci.sh` path as CI, with local concurrency from
`RSYSLOG_LOCAL_BUILD_JOBS` and `RSYSLOG_LOCAL_CHECK_JOBS`. For Valgrind-capable
check runs, set a non-sanitizer compiler and CFLAGS; ASAN-instrumented binaries
are not suitable for Valgrind tests.

For PR-ready validation with the actual changed-file set, apply the default
service relevance suppressions before entering the container. Local validation
normally happens before committing, so include committed branch changes,
staged/unstaged tracked changes, and untracked files. Clean generated build
state first so untracked build products do not pollute the relevance decision:

```sh
make distclean || true
: "${RSYSLOG_LOCAL_CHECK_JOBS:=10}"
: "${RSYSLOG_LOCAL_BUILD_JOBS:=10}"
export RSYSLOG_DEV_CONTAINER='rsyslog/rsyslog_dev_base_ubuntu:26.04'
export RSYSLOG_TESTBENCH_CHANGED_FILES="$({
  git diff --name-only origin/main...HEAD
  git diff --name-only HEAD
  git ls-files --others --exclude-standard
} | sort -u)"
export CC='gcc'
export CFLAGS='-g'
export CI_CONFIGURE_CACHE=1
export CI_MAKE_OPT="-j${RSYSLOG_LOCAL_BUILD_JOBS}"
export CI_MAKE_CHECK_OPT="-j${RSYSLOG_LOCAL_CHECK_JOBS}"
export CI_CHECK_CMD='check'
export VERBOSE=1
. devtools/apply-service-relevance.sh
rsyslog_apply_default_pr_service_suppressions
/usr/bin/time -p env \
devtools/devcontainer.sh --rm devtools/run-ci.sh
```

For service-skip validation with an irrelevant fake change set, set
`RSYSLOG_TESTBENCH_CHANGED_FILES` to a representative path such as
`doc/README.md` before sourcing `devtools/apply-service-relevance.sh`.

For a faster build-only smoke check, set:

```sh
CI_MAKE_CHECK_OPT="-j${RSYSLOG_LOCAL_CHECK_JOBS} TESTS="
```

This is useful for intermediate feedback, but it does not satisfy the PR-ready
validation gate.

Do not replace this run with a focused `TEST=...` lane just because the PR has a
single new test. The change-gated broad run validates the CI configure path,
generated state, service relevance filters, and broad testbench integration.

## Optional Tier 3: Risk-Triggered Specialist Lanes

Run these only when the changed area justifies the extra local time. The command
shape should stay close to `run_checks.yml`; use the trigger table above and
record deviations in the final status.

- **Memory-sensitive changes**: mirror `ubuntu_26_san` with `clang-21`,
  AddressSanitizer, UndefinedBehaviorSanitizer, disabled Valgrind, and the same
  configure extras. Use for parser, property, string, JSON, bounds, and
  segfault-related work.
- **Threading changes**: mirror `ubuntu_26_tsan` with `clang-21`,
  ThreadSanitizer, `--security-opt seccomp=unconfined`, and the CI TSAN
  suppressions. Use for queues, worker lifecycle, action shutdown, locks,
  shared state, and all changes to `runtime/wti.*` or `runtime/wtp.*`.
- **imtcp select-mode changes**: mirror `ubuntu_26_imtcp_no_epoll` when touching
  imtcp behavior that could differ between epoll and poll/select.
- **32-bit or ABI-sensitive changes**: rely on hosted i386 CI by default.
  Mirror `i386_CI` locally only when reproducing an i386-only failure, changing
  the i386 workflow/container, or when a maintainer explicitly requests local
  32-bit validation.
- **Distribution or build-system changes**: mirror `ubuntu_22_distcheck` for
  general dist/autotools risk, and `kafka_distcheck_CI` for Kafka build/test
  plumbing.

Do not run these by habit. The expected value is highest when the lane is tied
to the changed subsystem or to a specific failure mode seen during babysitting.

## Limited Local Environment Fallback

Some agents, workstations, and external contributor environments have limited
tooling. Web Agents commonly lack Docker/Podman, but the same situation can
happen on developer machines, locked-down CI shells, fresh containers, or
systems without optional linters. These environments are supported: missing
local tools are coverage gaps, not local workflow blockers. In that
environment:

- Prefer `devtools/local-validation-plan.sh` in default plan mode when a local
  checkout and shell are available. It can classify the change without starting
  containers.
- Use `devtools/local-validation-plan.sh --run` only when the selected class is
  runnable without containers, such as agent-doc-only, internal-doc-only,
  local-validation-tooling, or rendered-docs when the docs toolchain exists. Do
  not use a no-container `--run` result as substitute evidence for code,
  testbench, workflow, or focused container-test changes.
- Run every available applicable local check, such as diff-scoped linters,
  `actionlint`, pinned `zizmor`, Python style checks, shell syntax checks,
  documentation builds, host builds, focused host-side tests, and any
  repository scripts that work without containers. Missing one optional tool
  does not justify skipping other checks that are present.
- Inspect the changed files, `run_checks.yml`, and service relevance rules to
  identify the CI lanes that should cover the change.
- Do not claim PR-ready container validation. State clearly that container
  validation is unavailable in the current environment.
- Provide the exact local container lanes or hosted CI lanes that a container
  capable agent should run next.
- Missing local tools such as `shellcheck`, `checkbashisms`, `actionlint`,
  `zizmor`, `hadolint`, `trivy`, `jscpd`, `pycodestyle`, Cubic, Docker, or
  Podman are not fatal by themselves. Report the missing checks and continue
  with all deterministic checks that are available and relevant.
- For code changes, still run or request Cubic according to the Cubic rules
  when the local CLI is available. If Cubic is unavailable, report that as a
  separate AI-review limitation.

## Service Relevance

Regular PR CI may use approximate relevance gates to avoid scheduling expensive
service-backed test families that cannot reasonably be affected by the changed
files. The preferred result is that irrelevant families are absent from the
configured Automake `TESTS` set. Do not rely on starting an expensive test and
letting it skip late after service setup unless that is the only available
plumbing for that family.

Use `RSYSLOG_TESTBENCH_CHANGED_FILES` for local fake change sets when validating
the decision logic. Source `devtools/apply-service-relevance.sh` and call
`rsyslog_apply_default_pr_service_suppressions` before `devtools/run-ci.sh` to
apply the same PR-style configure suppressions in local container runs:

```sh
make distclean || true
export RSYSLOG_DEV_CONTAINER='rsyslog/rsyslog_dev_base_ubuntu:26.04'
export RSYSLOG_TESTBENCH_CHANGED_FILES='runtime/lookup.c'
export CC='gcc'
export CFLAGS='-g'
export CI_CONFIGURE_CACHE=1
export CI_MAKE_OPT='-j20'
export CI_MAKE_CHECK_OPT='-j10'
export CI_CHECK_CMD='check'
export VERBOSE=1
. devtools/apply-service-relevance.sh
rsyslog_apply_default_pr_service_suppressions
devtools/devcontainer.sh --rm devtools/run-ci.sh
```

For a fast scheduling proof, add `TEST_RUN_TYPE=MOCK-OK` and pass it into the
container via `DOCKER_RUN_EXTRA_OPTS='-e TEST_RUN_TYPE'`. The resulting
configure summary and `PASS:` list should show irrelevant heavy families
configured out rather than scheduled and skipped late.

For workflow or configure changes, validate both layers:

1. `tests/diag.sh module-needs-testing <family>` for representative changed
   files that should run and should skip the family.
2. A clean container/mock `devtools/run-ci.sh` path that confirms configure
   disables irrelevant test families and the generated test list omits their
   tests before execution.

The relevance check is intentionally conservative. Direct module changes,
related tests, testbench/build plumbing, workflow files, configure inputs, and
shared runtime paths that can plausibly affect a family must keep that family
enabled. Isolated helper-only changes, such as lookup tables or dynstats, may
avoid waking focused heavy families only when there is a clear code-path
rationale.

Full coverage must remain forceable. Use `RSYSLOG_TESTBENCH_FORCE_SERVICE_TESTS=1`,
`RSYSLOG_TESTBENCH_FORCE_<FAMILY>_TESTS=1`, or
`RSYSLOG_TESTBENCH_SKIP_SERVICE_RELEVANCE=1` when intentionally validating
service tests without relevant source changes, or when running daily, weekly,
release, flake-campaign, or maintainer-requested full coverage.

Allowed relaxations are narrow and tied to the touched area:

- imfile-only work may skip unrelated external service lanes.
- Kafka may be disabled only when neither Kafka modules nor Kafka test plumbing
  are touched.
- Elasticsearch may be disabled only when neither omelasticsearch nor
  Elasticsearch test plumbing are touched.
- Journal/imjournal runtime tests are static-analysis-only on hosts without a
  usable journal service. Do not try to force journal runtime tests in that
  environment; record the static-analysis exception instead.

Record every relaxation with the touched-area rationale. A skipped or relaxed
lane without that rationale makes the result targeted validation, not full
local container validation.

## Reporting

Report:

- exact command shape and container image
- image tag and image ID
- elapsed `real` time from `/usr/bin/time -p` where used
- pass/skip/fail summary from `tests/test-suite.log`
- whether skips were expected from relevance filtering
- lanes run, lanes relaxed/skipped, and touched-area rationale for each
  relaxation
- whether the result is fully container-validated or targeted
- unrelated local flakes separately from failures caused by the change

If a full container lane fails, inspect the logs and make up to four concrete
remediation attempts only when the failure appears locally fixable. Record the
command, log path, failure summary, each attempt, the affected PR/unit, and
whether host-side validation passed. If it still fails after those attempts,
carry the error forward in the session ledger instead of spinning.
