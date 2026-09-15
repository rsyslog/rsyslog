<!--
.. meta::
   :description: Execution ledger and evidence for the staged local queue implementation.
   :keywords: rsyslog, local queue, S0, S1, S2, validation, performance
-->

# Local queue implementation execution ledger

<!-- .. summary-start -->
The maintainer authorized autonomous S0 through S2 implementation on 2026-09-14,
with this session coordinating bounded subagents and independent stage reviews.
Stage acceptance requires the gates in the implementation plan. A build, review,
or favorable isolated timing does not alone establish acceptance.
<!-- .. summary-end -->

## Authority and immutable baselines

- [Design](local-queue-frontends.md), [plan](local-queue-implementation-plan.md),
  and [related work](local-queue-related-work.md): commit `8debb0a69`.
- Runtime baseline: `b0d9f971f007f06db3f734543cef5dfb312c5090`, verified against
  freshly fetched upstream/main on 2026-09-14.
- Work is confined to sibling worktrees; the coordination checkout is unchanged.
- No push or PR publication is part of the current execution step.

## Stage status

| Stage | Status | Evidence / next gate |
|---|---|---|
| S0 | Accepted | Astra/medium accepted contracts and baseline evidence; no optimization or PR-readiness claim. |
| S1 | Implemented; original performance gate unresolved | Reviewed attribution through `c9a51be21`; targeted correctness and sanitizer checks passed with documented debug-only TSan isolation. Maintainer authorized continued S2 work and methodology review. |
| S2 | Implemented; default container gate passed with documented specialist limits | Final runtime `536f5db06`: 1,595 passes, 69 skips, zero failures. Analyzer has one understood baseline warning; specialist outcomes and review limits below. Performance qualification remains deferred by maintainer direction. |
| S3 | Implemented; runtime/container gates passed; production qualification open | Bounded BE helping, independent concurrency review, sanitizer evidence and 4M/40M comparisons below. Final broad snapshot `2dac14fa2`: 1,604 passes, 69 skips, zero failures. |

## Assignments

- Coordinator: integration, evidence, configuration decisions, stage acceptance
  and maintainer discussion. Performance trials run without competing agent builds.
- `s0_contract_audit`, Terra/high: enqueue, lease, completion and shutdown audit.
- `s0_benchmark_audit`, Terra/high: existing harness and deterministic test oracles.
- `s0_config_contract`, Astra/high: option matrix and enforceable MVP boundaries.
- Existing `critical_queue_design_review`, Astra/medium: independent stage review.

## Validation environment

Docker is usable. The available Ubuntu 26.04 development image is
`rsyslog/rsyslog_dev_base_ubuntu:26.04`, image ID
`sha256:32ade478a405e4f27f077b5268ec5ecc59dd572843ad67ca2b6723594960ae09`.
The exact C formatter `clang-format-18` is installed. Local broad build/check
concurrency is 60 as specified by the machine overlay; benchmark concurrency is
specified separately by each workload. Targeted results are recorded below;
full implementation acceptance requires the remaining final gates.

## Measurement contract frozen before candidate timing

The S0 baseline uses GCC 15.2.0, `CFLAGS=-O2 -g`, with the same configure
options in both builds. Debug support is compiled in, with runtime debug logging
disabled during timing. This is an optimized diagnostic-capable build; no claim
of an NDEBUG production build is made. Compiler portability and sanitizer builds
are separate correctness lanes and never contribute performance timings.

Primary S2 metric: delivered messages divided by generation-plus-receiver-drain
seconds in the balanced, mutation-bearing imtcp contention workload. A 10 percent
throughput improvement corresponds to a candidate/baseline time ratio <= 1/1.10.
CPU reduction is diagnostic, not an alternative selected after timing. S1 is
neutral: no more than five percent throughput degradation in high and low
contention global controls (time ratio <= 1/0.95).

Use one calibration pair followed by eleven alternating recorded pairs, repeated
in a separate session. Record paired median and median absolute deviation (MAD).
A session is inconclusive if MAD exceeds 0.03 or the median's distance to an
acceptance boundary is less than twice MAD; this is a conservative campaign rule,
not a confidence interval. Repeat an inconclusive workload once under quieter
conditions before parking it. Exact-ID or process-completion failure invalidates
the trial and session; never keep its timing as accepted evidence.

Initial resource-matched reference: eight imtcp workers, sixteen connections,
eight FE slots of 10,000 messages each and two BE consumers with one million BE
slots. The MPMC comparison has ten queue consumers and 1,088,192 slots. Actual
producer registrations/started consumers must be reported: connections are not
FE identities and configured worker ceilings are not proof of started workers.
The bound includes eight active FE batches of 1024 entries; 1,080,000 would
match only waiting slots. Allocation bytes and RSS remain separate measurements.
A skew input uses one active connection but does not prove one actual imtcp
producer; callback ownership is assigned dynamically. A deterministic one-FE
skew test must establish registration identity separately in S2.

Low-contention control: one input worker and one active connection using global
mode, with identical candidate/baseline settings. Keep the existing imdiag
lifecycle screen as additional compatibility evidence, not the imtcp-only local
MVP comparison. Payload size is 512 bytes, dequeue ceiling 1024, minimum worker
activation threshold 1 for equal-budget comparisons. The first calibration will
choose a sufficient message count before measured candidate trials; record that
choice and do not vary it between compared revisions.

S2 latency guardrail: controlled healthy output at a fixed offered rate, p99
increase <= max(10 percent of baseline p99, 1 ms). Use one monotonic clock domain.
Blocked-output and finite skew-burst latency are separately reported stress
outcomes, not healthy-load guardrail samples. Generation/drain phase timings do
not establish per-message p99; a dedicated latency observation is required.

Stats-disabled and summary-enabled comparisons use matched collection settings.
Counters used for actual batch distributions and contention profiling run in
explicit diagnostic trials when they change overhead. A configured batch ceiling
is never reported as an observed batch size.

## S0 review corrections

- SC1: documented inherited failed segmented completion at worker termination;
  S1 preserves attribution without claiming to repair baseline final cleanup.
  Classic and segmented DA failure outcomes are now distinguished.
- SC2: repaired both benchmark fixtures to use two-argument `parse_json` and
  check status plus the expected nested mutation before producing each ID.
  `abortOnUncleanConfig=on` makes configuration errors fatal for timing.
- SC3: obligation-matched MPMC capacity is 1,088,192, including FE active batches.
  Earlier 1,080,000 calibration is waiting-slot matched only and not acceptance.

One-million-message calibration with corrected mutation fixtures completed exact
ID validation in both unchanged runtime builds; work time was approximately one
second. Freeze four million messages for measured balanced and skew sessions to
reduce phase-boundary quantization. No optimization result is implied.

Astra/medium verified SC1–SC3 as resolved and approved the S1/S2 contracts.
This is contract approval only; S0 measurement and validation gates are pending.
Baseline focused tests passed: `queue-minbatch.sh`,
`queue-minbatch-queuefull.sh`, and `queue-deferred-idle-shutdown.sh`.

## Conditional source preparation during S0 timing

The plan permits independent preparation before integration. While baseline
measurements run, isolated worktrees prepare S1 attribution and S2 ring/config
patches against the approved contracts. No competing builds/tests run during
measurements, and none of these prepared patches enters the measured runtime or
counts as an accepted stage. S1 integration remains conditional on S0; S2 routing
integration remains conditional on S1. This preserves the stage gates while
allowing source review and mechanical work to overlap baseline waiting time.

## S0 evidence collected

Normalized reports and exact binary hashes are under
[queue benchmark evidence](../../../benchmarks/queue-contention/evidence/local-queue-s0/).
Both measured runtimes still use baseline source `b0d9f971f`; later checkout
commits changed documents and the shared harness, not either measured binary.
The initial candidate build used checkout `8debb0a69`. Recorded checkout states
are retained separately from binary provenance.

| Unchanged-runtime control | Pairs | Median time ratio | MAD |
|---|---:|---:|---:|
| Balanced session 1 | 11 | 0.970646 | 0.026817 |
| Balanced session 2 | 11 | 0.989448 | 0.016864 |
| Low-contention session 1 | 11 | 1.001221 | 0.008113 |
| Low-contention session 2 | 11 | 1.003247 | 0.005416 |
| Connection-skew characterization | 11 | 0.998742 | 0.006026 |

All recorded trials passed exact delivery and clean shutdown. These are baseline
noise controls, not optimization results. Both balanced and low controls passed two independent sessions. The skew
session characterizes the unchanged MPMC baseline; it is not a candidate
performance acceptance comparison. No session exceeded the MAD noise threshold.

A separate impstats diagnostic accepted four million messages without queue-full
or discard events. Final main-queue snapshots recorded 1614–1657 contended
acquisitions and approximately 80–81 ms cumulative observed wait, with maximum
queue size 732011–757869. This counter covers `qqueueLock`, not every worker-pool
mutex acquisition; observed wait includes scheduling delay and is not hold time.
Concurrent validation builds were permitted for this diagnostic, so its durations
are excluded from performance interpretation. It does not establish that locking
dominates this workload.

The registered `queue-benchmark-oracle.sh` passed in the Ubuntu 26.04 dev image.
A separate checkout at `62ecf4804` passed configure, `make -j60 check
TESTS="queue-benchmark-oracle.sh"`, and `make distcheck TEST_RUN_TYPE=MOCK-OK
-j60`. Mock distcheck verifies packaging/build/distribution mechanics; it is not
a full test-suite run. No hosted AI review or final PR-ready container gate has
run for the eventual implementation; the work is **not fully
container-validated**.

A separate debugger-observed 20,000-message diagnostic passed exact delivery.
It observed 130 producer submissions of 16–230 messages and 90 nonempty
consumer acquisitions of 16–706 messages, plus 111 empty acquisition attempts,
under a dequeue ceiling of 1024. Both weighted histograms sum to 20,000.
This demonstrates variable actual counts and producer groups being split or
coalesced; debugger scheduling changes make it unsuitable as the uninstrumented
workload's batch distribution. Output request boundaries were not observed.

## S1 review findings under correction

During preparation, Astra caught an empty-dequeue regression: assigning the successful source-clear
return value replaced `RS_RET_IDLE` with `RS_RET_OK`. The implementation now
preserves the original result, pending rerun and independent verification.
Other corrections register the new header for distribution and permit a generic
completion callback argument to differ from the actual source. Source identity
must still match the worker pool and held mutex.

The reviewer requested actual queue-path tests beyond pointer helper tests:
discard-only acquisition, source-versus-decoy completion, retained segmented
completion failure, native DA attribution and worker cleanup. Failed completion
must retain its context and references. S1 does not claim to repair the inherited
terminal disk cleanup limitation described in the ownership audit.


## Prepared S2 components and pending validation

Preparation branches contain the SPSC primitive (`40d46828f`), strict shared
configuration enforcement (`f7b080671`), runtime integration checkpoint
(`d1e34b567`), snapshot stats adapter (`b5946879a` and `c9b45c19e`), daemon tests
(`649c8c5a6`), and latency harness (`098b090e5`, corrected by `abbfa9aeb`). These
are dependency components, not independently accepted stages or the main
integration branch. Component formatting passed; runtime and the latest test
revisions still need compilation/execution and integrated review.

Configuration review caught wide-number narrowing that could disguise an
unsupported setting. Qualification now retains raw numeric-domain failures,
including inherited legacy edge cases, without changing global-mode behavior.
It also rejects explicit custom parser instances: a built-in parser module can
have an instance configured to reroute messages. `queue.local.frontendStats` is
the opt-in bounded per-FE statistics flag; logical lifetime summaries remain
separate from resettable legacy arrival counters.

The independent SPSC review found no publication/wrap correctness defect. It
corrected a test that wrongly demanded a full pop despite cached consumer state,
added skip behavior for unsupported atomics and a concurrent-test watchdog, and
qualified the cache-spacing claim. Execution and sanitizers remain required.

Latency review corrected a malformed syslog stimulus, missing workload settings,
early termination of the exact-ID oracle, marginal rather than paired comparison,
and excessive observation jitter allowances. The observer now requires complete
post-shutdown output validation; source review alone does not establish a usable
measurement on this shared host.


## S0 acceptance

Astra/medium reviewed the complete normalized evidence and frozen contract and
accepted S0. No material S0 blocker remains. Balanced controls satisfy the
MAD < 0.03 noise rule and remain more than twice their MAD below the S1
throughput-degradation time boundary `1/0.95`; low controls are tighter.
This permits S1 integration, correctness testing and neutral-performance
measurement. It does not accept S1/S2 or establish a feature speedup.
The corrected fixed-rate latency observer and matched S2 latency baseline remain
S2 prerequisites, not a reason to delay the attribution-only S1 stage.


## S1 integration and validation

S1 is integrated from reviewed source `50ab62e2a` as `bb567e97a`; the one-line
allocation-helper cleanup from `c499ea212` follows without changing the measured
production path. The source owner passed the native lease unit, initialized-daemon
completion fixture, discard-only, out-of-order FixedArray, deferred idle/wakeup,
discard-allmark and native DA FixedArray tests, plus mock distcheck.

The first Ubuntu 26.04 matched-config run passed seven targeted tests. Its
out-of-order test could not start because that fixture needs omprog, which is
intentionally absent from the frozen S0 performance configuration. This is a
configuration coverage gap, not an observed queue regression. Run that fixture
with omprog in the separate correctness tree; do not change the performance
build's configure options to accommodate it. A separate S1 sanitizer/portability
worktree covers the threading/ownership paths. S1 is not accepted until its
correctness and paired neutral-performance gates pass.

## S2 review: interrupted Direct transactions

`COMM` can precede Direct transaction commit. `actionCommit` clears parameter
state even after cooperative `FORCE_TERM`, and `actionCommitAllDirect` ignores
that return, so checking the surviving parameter count cannot prove delivery.
The reviewed conservative local policy retains ambiguous `COMM` entries on
cancellation and callback return under source immediate shutdown, before private
state disposal and source completion; explicit discards remain terminal.
This can repeat delivery and script mutations. Remaining deadline-expired
obligations must be explicitly discarded/accounted in this memory-only stage.

A separate qualification prerequisite is cancellation safety of synchronous
omfile: its shared write mutex and stream state must remain reusable after an
interrupted write. A bounded local-only preparation/write/HUP boundary is under
implementation and independent review. Until that boundary and deterministic
cancellation tests pass, S2 output qualification is unresolved.

## S1 measurements and methodology review

The measured S1 candidate is `c9a51be21`, compared with the frozen S0 runtime.
Each session used one discarded calibration pair and eleven alternating pairs,
four million messages, and the previously recorded compiler/configuration.

| Session | Median candidate/baseline elapsed time | Ratio MAD | Original gate result |
|---|---|---|---|
| Balanced 1 | 0.9772665755 | 0.0734337015 | Inconclusive |
| Balanced 2 | 0.9744435943 | 0.0293889681 | Pass |
| Low load 1 | 0.9972346547 | 0.0054784295 | Pass |
| Low load 2 | 1.0018154705 | 0.0187414154 | Pass |
| Balanced 1, permitted rerun | 1.0564956619 | 0.0376971066 | Inconclusive |

These are whole-pipeline observations, including sender generation, TCP input,
JSON execution, shared synchronous omfile output and scheduling. They do not
isolate queue contention. No FE performance campaign has run yet. Preserve all
sessions, including unfavorable and inconclusive ones; S1 performance acceptance
under the original contract remains unresolved.

The corrected latency observer's baseline-only feasibility trial delivered all
100,000 messages at an offered 10,000 messages/second, with exact post-shutdown
payload/timestamp verification. Its maximum dispatch lateness was 4,302,031 ns
and reader iteration interval 7,746,632 ns, exceeding the frozen 400,000 ns
validity limits. This is an invalid latency measurement, not a queue latency
regression. Raw evidence remains in the session artifact directory.

On 2026-09-14 the maintainer clarified that disappointing benchmark results must
prompt examination of representativeness rather than abandonment of the design.
Continue S2 implementation and correctness validation while independently
reviewing the method. Investigate generator/output bottlenecks, actual submission
and dequeue shapes, sustained versus finite workloads, worker/resource matching,
and scheduling noise. Define the purpose and interpretation of revised
experiments before running them, and distinguish their results from the original
campaigns. Do not retroactively relabel the existing results as passing or relax
correctness requirements.

The maintainer further specified hundreds to thousands of clients, several
hundred thousand messages per minute, and larger production machines. Use those
absolute offered rates and connection populations for representative sustained
profiles. The existing sixteen-connection, four-million-message peak burst
remains a separate stress control. At a fixed offered rate below capacity,
sender-paced completion time is not a useful optimization target; compare CPU
per message, latency, backlog, actual batch shapes and routing, with identical
work offered to both implementations. Parameterize larger machine/worker
budgets, but do not infer their scaling from this heterogeneous WSL host.

The stated targets include Elasticsearch with material request latency and queue
buildup; the maintainer does not expect a faithful reproduction in this local
environment. Local performance work therefore remains diagnostic. Controlled
delay/failure tests establish queue mechanisms and ownership behavior, not
Elasticsearch throughput or bulk-request behavior. Continue the S2 MVP and its
correctness gates without claiming representative production performance.
Prepare a target-environment measurement recipe that correlates queue growth,
CPU/locking costs, actual batches, and Elasticsearch bulk-request sizes,
latencies, retries and completion rates. Production action qualification and
performance evidence remain explicit later work; existing failed/inconclusive
measurements retain their original classifications.

S1 targeted ASan/UBSan paths executed successfully with instrumented binaries.
The direct build did not apply CI compiler exclusion files; queue code was also
fully instrumented for TSan. Five focused TSan paths passed, but the debug-enabled
shutdown test stopped on a debug/imdiag startup race. The unchanged baseline
reproduced that same race in `debug.c:156` before queue stimulus. A temporary
debug-source-only compiler exclusion then allowed the deferred-shutdown and
native FixedArray DA paths to pass under TSan; queue, worker and test sources
remained instrumented. No broad queue exclusion or new tracked suppression was
added. These targeted checks do not establish full container validation.

Normalized S1 performance evidence, including the original classifications and
binary hashes, is stored in
[`benchmarks/queue-contention/evidence/local-queue-s1/`](../../../benchmarks/queue-contention/evidence/local-queue-s1/).


## S2 integrated checkpoint, 2026-09-14

Runtime assembly `5051c6d91` and the final integration-test corrections were
merged into the coordinator branch at `1fee0c63d`. S0/S1 evidence and the built
S1 performance control remain preserved. The combined benchmark tooling and
[target measurement method](local-queue-target-measurement-method.md) are
integrated at `beaee67c7`. No S2 performance campaign has run. The maintainer
reported a roughly 80-core machine with a very complex configuration; this is
provisional context, not a measured topology. Target configuration and
performance-data work is deferred to the next discussion.

The implemented scope remains the restricted, memory-only S2 MVP: local
main/ruleset queues, FixedArray backend, actual imtcp producer attribution,
preallocated bounded SPSC front ends, dedicated consumers, strict callback
qualification, and one logical queue with explicit physical-source ownership.
It does not qualify Elasticsearch actions or implement disk assistance,
backend helping, local action queues, or arbitrary queued graphs.

### Targeted evidence before the broad gate

All container results below use Ubuntu 26.04 image
`sha256:32ade478a405e4f27f077b5268ec5ecc59dd572843ad67ca2b6723594960ae09`.
Owner builds/checks used `-j10` each. These are focused container results, not a
completed PR-ready container gate.

| Area | Executed result | Limits |
|---|---|---|
| Configuration and allocation/registration failures | Seven tests passed, no skips | Both config frontends; actual producer retirement test requires epoll. |
| SPSC primitive and actual submission arrays | Unit passed; submission and REDIRECT tests passed at `2d6d80ee2` | Includes non-power-of-two capacity, variable arrays and publication race; not production throughput evidence. |
| Routing and lifecycle | Nine individual tests passed | FE/BE/FE reentry, partial batches, registration fallback, internal traffic, single/multiple FE shutdown, stats lifetime/reset and transactional replay. |
| Qualified omfile | Four actual tests passed in a clean VPATH build at `a9166037d` | Cancellation proves shared stream/lock reuse; unknown externally written prefixes can duplicate. |
| Production build guard | Testbench-disabled build passed at `2d6d80ee2` | Linked daemon/modules contain no local test symbols, fields, environment settings or fixture commands. |
| Distribution | Runtime mock distcheck passed at `2d6d80ee2`; combined harness mock distcheck passed separately | The fully merged archive is checked again in the final gate. Mock tests establish packaging, not runtime correctness. |

The reviewer found no concrete blocker in the corrected transactional seam.
The actual two-Direct-action test passed: action A observes mutation values
1 then 2 after replay, action B observes 2, and the real FORCE_TERM parameter
reset plus final ownership reconciliation are asserted. This FE-origin case
does not independently establish BE-origin interrupted-transaction coverage.

The Clang 21 no-debug lane initially rejected an alignment-increasing cast in
statistics registration. Commit `de10dce30` replaces byte offsets with typed
counter-array indices, preserving exported fields and their storage. The owner
reported the corrected compile passed; broader portability and sanitizer
results are recorded when complete.

Raw session evidence includes `/tmp/localq-failure-validation.md`,
`/tmp/rsyslog-s2-production-validation.md`,
`/tmp/local-queue-omfile-dist-real-tests.log`, and the owners' focused test logs.
These are local-session paths, not durable packaged artifacts. No hosted PR
review has run because no PR has been published. Final acceptance remains
pending the broad container check, analyzer, applicable specialist lanes,
current security receipt and late memory/concurrency/test-plumbing audits.


## S2 final qualification work

The final runtime checkpoint is `536f5db06`. The complete S0/S1 evidence remains
unchanged. The following refinements were made during integration and validation:

- `de10dce30`: typed statistics counter-array indexing for Clang portability.
- `1a83c7aa0`: remove an unread final shutdown-phase assignment reported by the
  static analyzer; deadline behavior is unchanged.
- `8da6e0301`: quote the assembled benchmark configuration as one `add_conf`
  argument and add an eight-message actual local imtcp/omfile exact-ID test.
  Earlier S0/S1 measured binaries and configurations are unaffected. No timing
  campaign ran with the broken new local-mode assembly.
- `60ae1a061`: use an atomic ordinal election in the cancellation test harness.
  The initial TSan report at its mutex-protected increment was not established
  as a C race; the subsequent blocking-read control explains the tool limitation.
- `e727da459`: independently exercise a cooperative BE-origin transaction
  interruption. With zero FE registrations and one active BE obligation, the
  real action FORCE_TERM/reset path leaves A written once and B empty. Final
  accepted obligations after the blocked snapshot must all be explicitly
  shutdown-discarded, accounting for late internal diagnostics. This closes
  the earlier BE-origin test gap without claiming successful BE replay or
  clean TSan coverage of asynchronous cancellation.
- `55118815f`: fix the inherited no-epoll inner FORCE_TERM path that retried the
  blocking poll after observing shutdown. The same imtcp-scoped deterministic
  test fails before and passes after this branch fix. It is not a general poll
  wakeup redesign.
- `43e7e4d19`: select the historical epoll teardown test only for epoll, and the
  new poll branch test for no-epoll. Both remain distributed. The independent
  reviewer verified original test intent and the limited meaning of this change.
- `536f5db06`: match the queue maximum counter's atomic reader with atomic
  writer accesses, retaining its existing writer mutex and enabled-statistics
  guard. There is no new lock or read-modify-write operation. The generic
  counter macro and unrelated modules are unchanged. Platforms where PREFER
  helpers fall back to plain access retain that inherited limitation.

### Sanitizer and portability outcomes

Ubuntu 26.04 ASan/UBSan passed twelve focused daemon cases and the subsequently
added BE transaction case. Leak detection was disabled in this lane; these
results do not establish absence of leaks. The ring primitive has separate
sanitizer evidence. Clang 21/NDEBUG and GCC 15/GNU23 debug builds passed. A final
Clang no-epoll, testbench-disabled object build covers the poll and atomic-counter
fixes and confirms that the test marker is absent.

TSan passed twelve non-cancellation S2 cases with only the stock runtime
suppression file. Queue, local queue, new statistics, and worker sources remained
instrumented; no compiler blacklist was used. The temporary `getIntValue`
suppression is absent from final runs after the maximum-counter repair. One
internal-message oracle timed out without a TSan diagnostic and passed an
isolated retry; all outcomes are retained.

Forced cancellation during blocking `read()` is excluded from the clean TSan
claim. A standalone two-thread control reproduces a reported counter race inside
cleanup despite both writers holding the same mutex; an analogous condition-wait
control passes. The production action creation/removal accesses also use the
same live mutex, confirmed by independent review. No production locking change
or tracked suppression was introduced to silence this report. Actual multi-FE
cancellation passes ordinary and ASan/UBSan testing.

### No-epoll limitations retained

The original no-epoll candidate run had 237 passes, 44 skips and one failure:
its pending-accept test exceeded a 15-second watchdog while the input waited
for the 60-second cancellation fallback. Nine standalone baseline attempts and
one full baseline run passed, while one of four paired candidate attempts failed.
The full baseline result was 220 passes, 44 skips, zero failures. The candidate
incidence remains unexplained; do not describe it as fixed or baseline-equivalent.
Source review found an inherited lost-wakeup window before blocking poll, but no
new signal-mask or cancellation-type change from local provenance wrappers.

After the applicability correction and deterministic branch fix, the selected
no-epoll suite again had 237 passes, 44 skips and one failure: an existing
`omfwd_fast_imuxsock.sh` exact-maximum assertion saw 97 instead of 100 despite
100,000 messages enqueued/sent and zero discarded. An unchanged rerun passed.
Four targeted tests passed after the atomic-counter fix: stats reconciliation,
stats lifetime, omfwd, and the new poll regression. These passes do not erase
the earlier full-run failures or establish an unconditional clean no-epoll lane.

The no-epoll variation enabled impstats and omtesting to exercise local queues;
other module selections followed the CI specialist lane. Its 44 skips were
missing PKCS11 tooling (40), missing GnuTLS CLI (1), tcpflood TLS-backend mismatch
(1), unavailable network namespace capability (1), and an existing TLS test under
review (1). Local-queue tests were not skipped; tests requiring distinct epoll
input workers were excluded by their applicability conditions.

### Review and reproducibility

Astra/medium applied `ai/rsyslog_memory_auditor/base_prompt.txt` and
`ai/rsyslog_bug_finder/base_prompt.txt`, then reviewed late deltas through
`536f5db06`. No new concrete memory/ownership/concurrency blocker was found.
The review explicitly retains the poll timing limitation and inherited disk
SC1 cleanup outside this memory-only MVP. The project-standards check verified
conditional test registration with unconditional distribution, executable scripts,
quoted configuration assembly and real runtime oracles. Changed C formatting,
ShellCheck, Python style, metadata, relative links and whitespace checks passed.
Advisory test-pattern matches were documented watchdogs, marker-synchronized
background senders that are waited, and diagnostic configuration assertions.

Detailed command/environment/source records and raw results are retained under
`/home/rger/rsyslog-local-queue-artifacts/s2-validation/`, including
`rsyslog-s2-sanitizer-evidence.md`, `rsyslog-s2-portability-evidence.md`, and
`rsyslog-s2-be-force-term-validation.md`. These are local session artifacts;
this ledger supplies the durable qualification summary. All use the Ubuntu
26.04 image ID above, with ten-way specialist builds/checks. No performance
campaign or hosted PR review was run during this qualification work.


### Final default container gate

At runtime `536f5db069f9abc57ae79bbe9c7be58fe1acec5e`, the final Ubuntu 26.04
`run-ci.sh` check completed in 401.72 seconds: **1,664 total, 1,595 passed,
69 skipped, zero failures/errors**. This is the broad default local container
gate, not merely the focused local-queue test selection. All selected new local
queue tests passed, including BE interruption and multi-FE cancellation.

The analyzer was run first on the same source and completed in 201.87 seconds.
Its command exited nonzero for one understood pre-existing
`tools/rsyslogd.c:1871` null-dereference report, unchanged from the baseline.
No new analyzer finding remains. This is not a claim of a zero-warning analyzer.
The container skill permits proceeding after findings are understood; the
subsequent broad check passed. The late memory/concurrency audit is recorded
above; security discovery covered all 76 expected source/test paths with no
introduced-or-worsened candidate. Its ignored, digest-bound receipt is finalized
after the final evidence documentation commit; no hosted AI review ran.

The broad lane applied the normal relevance helper against the frozen base,
with no forced TESTS selection and no additional module exclusions. Kafka,
MySQL, imfile, imtcp, imbeats, RabbitMQ and local queues ran. Elasticsearch was
built, but its service tests were disabled by the standard image's configuration,
not excluded by the relevance helper. The separate Elasticsearch CI lane and
actual target qualification remain outstanding. Image-default imdocker test
exclusion also remains. Skips include missing PKCS11 tools (40), Azure DCE
endpoint/credentials (11), namespace capabilities (3), raw-socket/root
requirements (4), and eleven other environment/configuration prerequisites.
The preserved skip inventory names every test.

An earlier broad run at `8da6e0301` had 1,593 passes, 69 skips and one failure
in 447.48 seconds. `omprog-restart-terminated-vg.sh` observed 46 descriptors at
start and 45 at end, with a clean Valgrind result and daemon exit. Its unchanged
isolated rerun passed in 11.37 seconds; the final full run also passed it. Keep
this transient, non-reproduced result without claiming proven baseline or
local-queue causation.

The combined mock distribution run at `43e7e4d19` passed in 83.98 seconds:
1,519 mock passes, ten skips, zero failures, with archive, out-of-tree build,
installation/uninstallation and cleanup checks. It includes every new file/test.
The later queue-maximum fix changes no manifests and is covered by the final
broad build/run. Mock results are packaging evidence, not daemon test evidence.

Exact broad command shape (clean dedicated validation worktree):

```sh
export RSYSLOG_DEV_CONTAINER='rsyslog/rsyslog_dev_base_ubuntu:26.04'
export RSYSLOG_TESTBENCH_CHANGED_FILES="$(git diff --name-only b0d9f971f007f06db3f734543cef5dfb312c5090...HEAD | sort -u)"
export CC=gcc CFLAGS=-g CI_CONFIGURE_CACHE=1
export CI_MAKE_OPT=-j30 CI_MAKE_CHECK_OPT=-j30 CI_CHECK_CMD=check VERBOSE=1
. devtools/apply-service-relevance.sh
rsyslog_apply_default_pr_service_suppressions
/usr/bin/time -p devtools/devcontainer.sh --rm devtools/run-ci.sh
```

The source checkout was clean; subsequent changes only record internal evidence.
Thirty-way broad concurrency plus ten-way specialist jobs stayed within the
machine's 60-way aggregate capacity. The explicit image tag and immutable image
ID are given above. The full analyzer command was:

```sh
/usr/bin/time -p env \
  RSYSLOG_DEV_CONTAINER=rsyslog/rsyslog_dev_base_ubuntu:26.04 \
  SCAN_BUILD=scan-build SCAN_BUILD_CC=clang \
  SCAN_BUILD_REPORT_DIR=scan-build-report CI_MAKE_OPT=-j30 \
  DOCKER_RUN_EXTRA_OPTS='-e SCAN_BUILD -e SCAN_BUILD_CC -e SCAN_BUILD_REPORT_DIR' \
  RSYSLOG_CONFIGURE_OPTIONS_EXTRA='--disable-elasticsearch --disable-elasticsearch-tests --disable-imkafka --disable-omkafka --disable-kafka-tests --disable-mysql --disable-mysql-tests' \
  devtools/devcontainer.sh --rm devtools/run-static-analyzer.sh
```

These analyzer module exclusions match its documented lane. The combined mock
lane used the same image with `CI_MAKE_OPT=-j30`, `CI_MAKE_CHECK_OPT=-j30`,
`CI_CHECK_CMD=distcheck`, `TEST_RUN_TYPE=MOCK-OK`,
`DOCKER_RUN_EXTRA_OPTS='-e TEST_RUN_TYPE'`, `ABORT_ALL_ON_TEST_FAIL=YES`,
`VERBOSE=1`, and `/usr/bin/time -p devtools/devcontainer.sh --rm devtools/run-ci.sh`.
Full commands, logs and exact source provenance are preserved in
`rsyslog-s2-final-container-evidence.md` in the artifact directory above.

The default PR-ready local container sequence is complete with the understood
baseline analyzer warning and current source reviews; the separate no-epoll
full-run discrepancies and forced-cancellation TSan limitation are explicitly
not described as passing. Performance acceptance and broader production action
qualification remain deferred. No push, PR, deployment or release was performed.

## S3 entry measurements, 2026-09-15

The maintainer authorized S3 after collecting this S2 matrix. These are exploratory three-pair comparisons, not acceptance under the original eleven-pair/two-session qualification contract. Security review is postponed until the S3 candidate is stable.

The original global baseline is `b0d9f971f007f06db3f734543cef5dfb312c5090`; the S2 candidate is `54dbe93612049a717920748325acdc52bb83459e`. The latter includes the empty-BE shutdown-pointer regression fix. Its focused sanitizer checks passed, but two subsequent broad container runs each failed a different test; both tests passed in isolation. Therefore this entry does not extend the earlier full-container claim to that candidate.

Each configuration uses one discarded calibration pair and three measured pairs with alternating order. All 64 trials passed process completion and exact-ID verification. Input remains eight imtcp workers, sixteen TCP connections, 512-byte payload, JSON parsing/mutation and shared synchronous omfile; dequeue upper limit 1024 and worker activation minimum 1. The global queue retains ten consumers and 1,088,192 slots. Every local scenario has at most eight FEs and a one-million-slot BE. FE100K increases memory capacity; BE10 increases the total worker ceiling from ten to eighteen. Actual worker counts and batch distributions are not inferred from these settings.

| Messages | FE slots each | BE workers | Global seconds | S2 seconds | Paired S2/global time | Ratio MAD |
|---:|---:|---:|---:|---:|---:|---:|
| 4,000,000 | 10,000 | 10 | 3.55 | 2.96 | 0.8362 | 0.0206 |
| 4,000,000 | 10,000 | 2 | 3.53 | 4.70 | 1.3327 | 0.0127 |
| 4,000,000 | 100,000 | 10 | 3.56 | 3.72 | 1.0481 | 0.0048 |
| 4,000,000 | 100,000 | 2 | 3.55 | 3.68 | 1.0297 | 0.0001 |
| 40,000,000 | 10,000 | 10 | 34.03 | 30.62 | 0.8997 | 0.0067 |
| 40,000,000 | 10,000 | 2 | 33.37 | 36.24 | 1.0855 | 0.0052 |
| 40,000,000 | 100,000 | 10 | 30.90 | 29.61 | 0.9540 | 0.0039 |
| 40,000,000 | 100,000 | 2 | 31.73 | 34.99 | 1.0893 | 0.0106 |

Seconds are medians from sender launch through the receiver completion barrier. Shutdown and exact-ID verification are required but outside this processing metric. Ratios are medians of paired ratios, not ratios of table medians. GCC 15.2.0, `-O2 -g`, runtime debug logging off; same pinned Ubuntu 26.04 image as above. No owned build/test jobs overlapped timing; the host and caches remain non-exclusive. The per-trial safety timeout was increased to 300 seconds to accommodate 40M delivery verification.

Interpretation: at 40M, FE100K alone did not improve the BE2 comparison relative to global. Ten BE workers improved elapsed time, with additional worker capacity. The shorter runs magnify buffer/drain effects. These results motivate testing S3 at the same worker budget; they establish neither a production Elasticsearch speedup nor per-message tail latency.

Reproduction: use `benchmarks/queue-contention/compare.py --workload multi --pairs 3 --messages {4000000,40000000} --input-workers 8 --connections 16 --payload 512 --worker-minimum 1 --dequeue-batch-size 1024 --before-queue-size 1088192 --before-consumer-workers 10 --after-scope local --after-max-frontends 8 --after-queue-size 1000000 --after-frontend-size {10000,100000} --after-consumer-workers {2,10} --trial-timeout 300`, supplying isolated `--before`, `--after`, and unique `--output` paths for each Cartesian-product scenario. Braces here denote alternatives, not a directly executable shell command.

Raw commands, metrics, logs, and the normalized `s2-summary.json` are preserved under the coordinator artifact directory `s3-campaign`. The frozen S2 build was copied before S3 edits; its rsyslogd SHA-256 is `c441f34e92fde354b91eb5168f7e69c0ae1c9e53918e47166c6ab74a1729a86a`. No S3 implementation or validation claim is made by these S2 measurements.

## S3 helping checkpoint and measurements, 2026-09-15

Runtime checkpoint `005d661b00859f5f7f1f50800c1e914ae99dec46` adds bounded memory-BE helping. Test-only follow-ups are `8faa24cf8822599519fb7c0838e20946bf9ff21c` (gate eligibility) and `4cce311ac4b016560b2fe668ec10dcafc9b7a9cd` (cooperative borrowed replay). The final optimized binary was refreshed at runtime source `8faa24cf`; the later commit changes only fixtures. No S4 transaction/graph coverage, disk support, or production qualification is implied.

The independent Astra/medium concurrency review found a selected-helper wake handoff defect: local work could take priority without waking another idle helper for pending BE work. A pending-selection flag and deterministic two-FE regression fixed it. The reviewer also required borrowed transactional interruption and partial-batch oracles; final R1–R3 verification closed all three findings. Generic lease binding remains strict; explicit helper APIs keep FE pool identity separate from BE completion ownership.

### Same-resource S3 versus S2 performance

The same method and matrix as above now compare frozen S2 `54dbe936` against S3. Each configuration has a discarded calibration pair plus three alternating measured pairs. All 64 final trials passed exact delivery and process completion. An earlier eight-trial FE10K/BE2 screen at `005d661b` also passed (time ratio 0.7499); the table uses only the final campaign.

| Messages | FE slots each | BE workers | S2 seconds | S3 seconds | Paired S3/S2 time | Ratio MAD |
|---:|---:|---:|---:|---:|---:|---:|
| 4,000,000 | 10,000 | 10 | 2.89 | 2.83 | 0.9810 | 0.0038 |
| 4,000,000 | 10,000 | 2 | 4.60 | 3.47 | 0.7516 | 0.0045 |
| 4,000,000 | 100,000 | 10 | 3.69 | 3.62 | 0.9777 | 0.0106 |
| 4,000,000 | 100,000 | 2 | 3.66 | 3.59 | 0.9815 | 0.0120 |
| 40,000,000 | 10,000 | 10 | 27.98 | 27.87 | 0.9953 | 0.0015 |
| 40,000,000 | 10,000 | 2 | 33.53 | 32.50 | 0.9660 | 0.0046 |
| 40,000,000 | 100,000 | 10 | 29.43 | 29.39 | 1.0009 | 0.0011 |
| 40,000,000 | 100,000 | 2 | 34.42 | 33.36 | 0.9633 | 0.0004 |

The main short-run FE10K/BE2 improvement is 24.8 percent shorter elapsed time; its 40M improvement is 3.4 percent. FE100K/BE2 improves 3.7 percent at 40M. Both BE10 long-run comparisons are effectively neutral. These are exploratory results, not formal qualification or evidence for remote Elasticsearch workloads. The smaller long-run effect is consistent with final drain contributing less to total elapsed time. Absolute medians across separate campaigns must not be used as a new global/S3 comparison: host performance drifted; paired controls are authoritative.

For reproduction, use the previous command shape with the frozen S2 tree as `--before`, `--before-scope local --before-queue-size 1000000 --before-frontend-size F --before-max-frontends 8 --before-consumer-workers W`, matching F/W on the S3 side. The final campaign uses a separate output directory for each configuration. Commands and normalized `s3-summary.json` are preserved in `s3-campaign`.

A separate, untimed-interpretation diagnostic enabled family and per-FE stats at FE10K/BE2. It observed eight FE workers, two BE workers, and helper retirement of 403,456 BE messages in 394 batches (observed maximum 1024). All 4M messages were terminal, zero outstanding, helper completed=terminal=403,456 and returned=0. Eight per-FE snapshots were present. This confirms helping was active; it is not a latency measurement or a fixed-batch assumption.

### Correctness and sanitizer evidence

Ordinary optimized container tests passed configuration parity, priority, bounded capacity release, three idle/wake interleavings, partial helper batches, cooperative interruption, actual cancellation, and selected S2 shutdown/submission regressions. Nine new regression scripts are registered in the existing test-owning subtree. Source/configuration details and exact commands are retained in `s3-focused-evidence.md` beside raw logs.

ASan/UBSan covered 13 distinct focused tests successfully, including forced borrower cancellation. The first sanitizer run exposed a fixture-only startup race: an after-registration gate could fire before the first local message completed while the shell waited for that completion. Requiring initial FE retirement before taking the test gate fixed the circular wait. Before-registration and handoff variants passed again; after-registration passed three repetitions. No production protocol change or sanitizer suppression was needed.

TSan covered 12 distinct focused tests successfully, including cooperative borrowed replay and stats lifetime/reconciliation. The forced-read-cancellation path reported missing mutex synchronization during cleanup. The same clang-21/image reproduced that report in the independent minimal read-cancel control despite explicit mutex locking; the normal cleanup control passed. Current dedicated BE cleanup and main-thread borrowed retirement were traced to the same BE mutex. The forced-cancellation TSan path therefore remains a tooling coverage limit, not a passing test claim. Existing ordinary/ASan forced-cancellation tests remain intact; no queue suppression was added. A separate cooperative replay variant exercises real FORCE_TERM and borrowed BE return under TSan without that interceptor path.

The pinned Ubuntu 26.04 image and compiler flags are recorded in `sanitizers.sh`; ASan uses clang-21 address/undefined checks, TSan uses clang-21 with stock `tests/tsan-rt.supp`. Leak detection is disabled, so there is no whole-daemon leak claim. Final refresh/sanitizer builds used `-j60`; original author-focused builds/checks used explicit `-j10`. C formatting, ShellCheck, and diff whitespace checks passed. Full container, distribution, production/portability, and security gates are recorded separately after this checkpoint; they are not implied by the focused results above.

### Final S3 container validation

The final immutable test snapshot is `2dac14fa2f682ecae9b798b09593d5ed7d437856`; runtime remains unchanged from the measured S3 code. Ubuntu 26.04 change-gated `run-ci.sh` passed: 1,673 total, 1,604 PASS, 69 SKIP, zero FAIL, 346.19 seconds, build/check `-j60`, same image ID recorded above. Conservative PR relevance rules were retained. The analyzer (`-j20`, 208.99 seconds) reported only the known baseline `tools/rsyslogd.c:initAll:1871` warning and no S3 issue. Clang21/no-debug and GCC15/GNU23-debug production builds with testbench disabled passed (build phases 16.89 and 17.76 seconds, `-j20`). Updated mock distcheck passed in 86.11 seconds.

Earlier broad attempts are not hidden: the first had four failures (1,600 PASS/69 SKIP), the second one (1,603 PASS/69 SKIP). Two fixture assumptions were corrected in `f35c8cb2`: the held wake gate must release after any BE pending admission, not a particular producer; and two TCP connections do not establish two producer identities. The internal-message fixture now constructs one FE and one BE callback explicitly. TLS readiness and segmented-DA event-property tests passed isolated checks. The remaining HUP fixture assumed BE routing selected a fresh, healthy dedicated worker; S3 helpers preserve their existing action suspension state. `2dac14fa2` pins that S2 HUP ownership oracle to helper limit zero, preserving failed-open/no-lazy-open/recovery checks. The final broad rerun then passed. These fixes change tests, not the measured runtime.

Exact commands, per-test failures, image identity, source equivalence, isolated checks and final verdict are retained in `s3-campaign/final-validation`. Late memory/concurrency prompt audits are in `late-audit.md`; the independent concurrency review and TSan control evidence remain separate. The security review is a final digest-bound artifact in ignored `.codex/security-review/`; check its matching receipt before claiming PR readiness. Hosted checks/review threads still refer to the previously published PR head, and no push is part of these S3 measurements or local validation claims.
