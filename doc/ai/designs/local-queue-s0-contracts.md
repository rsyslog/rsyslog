<!--
.. meta::
   :description: Draft S0 activation, producer, resource, and queue option contracts for the experimental local queue MVP.
   :keywords: rsyslog, local queue, S0, S2, activation, producer registration, option matrix
-->

# Local queue S0 contract draft

| Metadata | Value |
|---|---|
| Status | **Draft for coordinator and architecture review; no runtime implementation or qualification** |
| Recorded | 2026-09-14 |
| Source baseline | `8debb0a69` |
| Authority | [Design](local-queue-frontends.md), [implementation plan](local-queue-implementation-plan.md) |
| Scope | S0 producer, activation, and resource decisions needed by S2; lease details require the companion ownership audit |

<!-- .. summary-start -->
Propose a fail-closed memory MVP: bounded per-thread fronts, shared BE overflow,
trusted imtcp producer registration, audited Direct actions, and explicit
configuration rejection before activation. These are proposed implementation
contracts, not implemented behavior or evidence of runtime correctness.
<!-- .. summary-end -->

## 1. Decisions and ownership

The coordinator owns acceptance of every row. The implementing agent owns its
tests; an architecture reviewer challenges the integrated result. These choices
can be resolved within the authorized S0–S2 scope without a new product decision.
Names below are proposed, not available configuration parameters.

| Decision | Proposed S2 behavior | Rejected alternative | Required oracle |
|---|---|---|---|
| O1 | `queue.scope="global"` default; `"local"` explicit experimental mode. Local main/ruleset queues use FixedArray initially; LinkedList can be enabled only after the same gates pass. Local action and Direct/disk queue scope rejected. | New queue type; silently converting Direct into asynchronous execution | C01 both frontends and inherited defaults |
| O2 | `queue.size=B` remains the BE physical bound. Require positive `queue.local.frontendSize=F` and `queue.local.maxFrontends=N`. Report a checked aggregate bound including active holdings. Reference values are F=10,000 and B=1,000,000; N is explicitly bounded. | Hidden multiplication of an old logical limit; an unbounded TLS registry | C02 capacity and integer-overflow rejection; C03 full FE→BE→FE reuse |
| O3 | Preallocate N registration descriptors and rings; claim once per actual producer thread and queue. No descriptor reuse before queue destruction. Retain producerless fronts and their consumer until shutdown. Registration/start failure or lifetime cap exhaustion uses BE. | Listener/session identity; pthread ID reuse; replacing producerless registrations before proving cache lifetime | C04 producer exit/replacement/cap/allocation and worker-start failure |
| O4 | Portable predicate/recheck/condition-variable FE wakeup; no producer parks for FE space. Whole-batch no-fit selects BE and its established wait policy. | Busy spin; cached-full routing without an acquire refresh; FE-space waits that bypass BE | C05 empty→park/publication and fresh-capacity interleavings |
| O5 | At most one bounded unresolved lease per FE worker; retain its message references and module state; do not acquire another lease or publish retry into the SPSC. | Consumer re-enqueue to its own FE; unbounded retry list | L01 companion lease contract and cancellation tests |
| O6 | After producer quiescence, stop/join FE execution before a maintenance owner transfers residual obligations to still-running BE; failed transfer keeps responsibility until explicit terminal memory-shutdown discard. | Freeing FE before transfer outcome; resubmission through external-admission accounting | L02 partial-transfer/cancellation/full-BE shutdown |
| O7 | One dedicated consumer per registered FE, plus configured dedicated BE worker pool; no helping. Record N as allocation cap and actual registered/started worker counts. | Calling extra workers a pure queue-lock improvement | P01 equal-worker/equal-capacity paired measurements |
| O8 | Apply the exhaustive matrix below, with explicit-used flags retained where inactive defaults differ from requested unsupported features. | Silent acceptance of options the FE path cannot honor | C01 one rejection test per option family |
| O9 | Only trusted imtcp execution obtains FE registrations. Internal and unclassified runtime submissions use BE without an FE and are counted separately. Reject unsupported configured inputs and script paths before startup. No reload. | Trusting `$inputname`; allowing an unsupported input merely because fallback exists | C06 spoofed input name, internal diagnostics and unknown producer; C07 graph escapes |
| O10 | No DA and no borrowing in S2. | Treating existing DA child pointers as FE ownership | C01 disk/DA rejection |
| O11 | One logical monotonic shutdown deadline per phase across all FEs and BE, not N serial full timeout allowances. Cancellation completion stays source-bound. | Timeouts multiplied by FE count; freeing callback state while callback executes | L03 N-front timeout, cancellation, late internal messages |
| O12 | Preserve existing `enqueued` as arrival attempts; expose accepted, terminal, FE/BE queued and active inventories separately. Transfers do not increment external arrivals. | Relabeling historical attempts as accepted deliveries; adding tiers twice | C08 quiescent reconciliation and stats reset/concurrent scrape tests |

## 2. Strict activation support

### 2.1 Shared validation placement

Add a hard `CHKiRet`-propagating validation call in `rsconf.load`, after
`tellModulesCheckConfig`, `validateConf`, and `loadMainQueue`, and before the
configuration-verification early return. At that point both frontends have
produced the shared objects, ruleset optimization has run, imtcp bindings have
been resolved, and the main queue exists. Validate the final effective values,
including inherited legacy/module defaults, rather than just explicit syntax.

Maintain per-queue explicit-parameter flags during `qqueueApplyCnfParam` and retain
any local validation error until the hard gate. `createMainQueue` currently ignores
the return from `qqueueApplyCnfParam`; fixing that propagation is necessary but
insufficient without final graph validation. `tellModulesCheckConfig` intentionally
returns success even for a module it cannot activate. Do not put the only local
queue guard there. `startMainQueue` also retries failed startup as Direct: reject
that downgrade for a local candidate, or fully dismantle the local candidate and
fail startup. It must not turn into an unaudited execution mode.

The caller in `rsyslogd.init` ordinarily treats only missing configuration and
no-actions results as unconditional load failures. A new generic error returned
by `rsconf.load` alone therefore does not prove that startup stops. Introduce a
dedicated local-support failure result recognized as fatal by that caller, or a
persistent fatal-validation flag checked there. Do not let
`AbortOnUncleanConfig=off`, partial-config verification, or the developer
keep-running-on-hard-errors option bypass this local runtime safety gate.

Keep a defensive checked state on the queue so `qqueueStart` cannot activate a
local queue that missed the graph gate. An unsupported module/option is a hard
configuration failure, including `-N1`; partial configuration success must not
activate the remaining portion of a local graph.

Source anchors: [rsconf.c](../../../runtime/rsconf.c) `load`,
`tellModulesCheckConfig`, `loadMainQueue`;
[rsyslogd.c](../../../tools/rsyslogd.c) `createMainQueue`, `startMainQueue`;
[queue.c](../../../runtime/queue.c) `qqueueApplyCnfParam`, `qqueueCorrectParams`.

### 2.2 Queue and ruleset reachability

For the first implementation, conservatively reject configured non-imtcp input
modules whenever local scope is present, except specifically recognized
out-of-band instrumentation that cannot enqueue messages. For example, impstats
would require `log.syslog=off` and an audited independent file path before being
excepted. A test build can admit imdiag only through an explicit test capability;
its injection path must use BE and must never acquire an imtcp FE registration.
Loading an input for metadata alone can be rejected conservatively in this MVP.

Validate every ruleset whose effective queue is local, and every queue-less
ruleset when the main queue is local. `ruleset.processBatch` selects the script
from each message's `pRuleset`, falling back to the default ruleset; validation of
the default root alone misses nondefault rulesets sharing main Q. The S2 gate may
conservatively validate all rulesets in a configuration containing local scope
and reject all ruleset calls throughout that configuration. This prevents an
otherwise-global queue from calling into a local root before graph shutdown is
implemented. Root queue declaration order then has no dependency significance.

Proposed AST whitelist:

- Accept `S_NOP`, `S_STOP`, and qualified `S_ACT`.
- Accept `S_PRIFILT` and non-regex `S_PROPFILT`, recursively validating both
  branches. Accept `S_IF` only with literals, property reads, and a fixed whitelist
  of ordinary arithmetic/boolean/comparison operators; recursively validate both
  branches and chained conditions.
- Accept `S_SET` and `S_UNSET` only for message/local variables, with the same
  expression whitelist. Reject global/shared-variable mutation.
- Reject `S_CALL`, `S_CALL_INDIRECT`, `S_FOREACH`, `S_RELOAD_LOOKUP_TABLE`, every
  function-call expression, and unknown future statement/expression nodes.
  The sole initial built-in function exception is `parse_json` on two literal
  arguments (JSON and a destination restricted to a message-local `$!` path), needed by the mutation-bearing contention workload; resolve it to
  the audited built-in function identity, validate both arguments and test JSON
  allocation/error paths. Do not accept function names generally.

Function rejection matters: `http_request`, `exec_template`, and module-registered
functions are reachable inside expressions and can defeat a statement-only
whitelist. The existing action iterator skips calls and is not a reachability
validator. Use the optimized AST for execution coverage; include diagnostics that
name the local queue, ruleset and offending statement/action. YAML and
RainerScript rejection fixtures must describe the same semantics.

Source anchors: [ruleset.c](../../../runtime/ruleset.c) `scriptIterateAllActions`,
`scriptExec`, `processBatch`, `GetRulesetQueue`, `rulesetProcessCnf`;
[rainerscript.h](../../../grammar/rainerscript.h) `cnfstmt`, `cnffunc`;
[rainerscript.c](../../../grammar/rainerscript.c) `addMod2List`.

### 2.3 Callback whitelist and transactional qualification

Direct queue type alone is insufficient: `action.isTransactional` is derived
from module capability independently of queue type. omfile implements
`beginTransaction` and `commitTransaction` even with a Direct action queue.

Proposed callback entries, enabled only after their corresponding tests pass:

| Entry | Allowed configuration and limits | Qualification |
|---|---|---|
| Controlled test action | Audited omtesting modes for deterministic blocking/error injection; Direct; no extra action suspension/rate-limit/filter policy outside the case under test | Test-only. Existing omtesting shares a mutex and optional stdout writes; it is not a lock-free performance baseline |
| omfile fast sink | Static `/dev/null`, Direct, ordinary property/string template; no dynamic file, async writer, compression, rotation command, signing, encryption or sync | Narrow transactional exception; useful contention screen, but cannot independently prove received message IDs |
| omfile ID sink | Static test file on controlled local storage, Direct, async writing off, flush-on-transaction-end on, same excluded features as above | Narrow transactional exception; receiver/file ID oracle plus short/partial transaction, error and shutdown tests required |
| Discard/stop | Explicit terminal discard behavior; builtin discard only if retained through optimizer | Useful terminal-ownership case, not successful output-delivery evidence |

No generic production output module is qualified by this draft. omfile file
writes can block and its shared `mutWrite` can dominate throughput. Do not call it
nonblocking. `/dev/null` has an explicit early return in `commitTransaction`, but
still exercises core transaction accumulation. Qualification must cover varying
FE/BE dequeue counts, cancellation around commit, and source-specific lease
completion. A blocked/error case must use a deterministic test barrier rather
than relying on random filesystem delays. Keep requests/transaction sizes in the
performance report.

For normal callback qualification, reject action rate-limit policies, execute-Nth
or interval filters, previous-action-suspended conditionals, external state/error
files, and indefinite action resume retries. Preserve default ordinary behavior
only after the effective action settings are audited. Module internals are opaque
to the core: implement an explicit module-specific validation helper/capability
for omfile and omtesting, inspecting resolved instance fields. A core check of
module name plus original `pSyntaxLst` cannot catch inherited legacy defaults.
No new broad public module capability is required for this MVP.

Source anchors: [action.h](../../../runtime/action.h) `action_s`;
[action.c](../../../runtime/action.c) module transaction selection and
`actionCommit`; [omfile.c](../../../tools/omfile.c) `setInstParamDefaults`,
`beginTransaction`, `commitTransaction`; [omtesting.c](../../../plugins/omtesting/omtesting.c)
`doAction`; [omdiscard.c](../../../tools/omdiscard.c) `doAction_NoStrings`.

## 3. Producer provenance and lifetime

The existing imtcp plugin sets `tcpsrv.SetOrigin(..., "imtcp")` separately from
`SetInputName`, whose value users can configure. imgssapi sets its origin to
`"imgssapi"`. Use the configuration-fixed server origin plus the activation input
whitelist as the trusted internal provenance; never identify a producer from a
message property or a thread's printable name.

Tag actual executing threads inside runtime tcpsrv `Run` and `wrkr`, only for the
audited imtcp origin. This preserves imtcp plugin code and submission calls.
`Run` covers single-worker epoll, poll fallback and extra imtcp server threads;
`wrkr` covers the tcpsrv I/O pool. An origin tag is eligibility, not FE identity.
Allocate a unique registration generation per actual thread and logical queue,
using a thread-local cache for successful registrations. A multi-submit belongs
to the current executing thread, not to its TCP session.

Do not let diagnostic recursion inherit FE eligibility: `INTERNAL_MSG` goes to
BE regardless of the thread tag. Unclassified submissions also go to BE without
creating a registration. This is a documented preservation path for messages the
static gate cannot classify, not support for arbitrary input modules. Count
internal, unknown, capacity-exhausted and registration-failed fallback separately.

Preallocate a bounded registry and ring storage before input startup. Claiming a
descriptor and starting its one FE worker is a cold-path locked operation. A
failed worker start marks that descriptor unusable and routes the submission to
BE; do not publish into an FE without a live consumer. Entries are never reused
during S2. Producer exit marks the entry producerless and signals the consumer.
The consumer drains normally; retaining it until logical shutdown bounds resource
use by N and simplifies lifetime. Replacement producers use unused entries or BE
when the lifetime cap is exhausted. Report this limitation explicitly.

TLS cleanup must never touch freed queue state. Because only imtcp threads own
FE cache entries in S2, queue destruction waits for these producers to leave their
tagged scopes and for input thread joins; shutdown also closes registration and
waits for any routing/publication critical section to exit. Clear thread cache
entries while the queues are still alive. A TLS destructor is a backstop, not the
sole quiescence proof. If general producers or live reload are later admitted,
this lifetime proof must be replaced or extended before enabling them.

Source anchors: [imtcp.c](../../../plugins/imtcp/imtcp.c) `addListner`,
`RunServerThread`, `runInput`; [tcpsrv.c](../../../runtime/tcpsrv.c) `Run`, `wrkr`,
`processWorkset`, `startWrkrPool`, `SetOrigin`;
[tcpsrv.h](../../../runtime/tcpsrv.h) `pszOrigin`;
[rsyslogd.c](../../../tools/rsyslogd.c) `logmsgInternalSubmit`, `processImInternal`,
`deinitAll`; [tcps_sess.c](../../../runtime/tcps_sess.c) `DataRcvdUncompressed`.

## 4. Bounded resources and accounting

### 4.1 Partitioned credits

Choose bounded per-tier reservations instead of a contended aggregate atomic
credit counter on every FE admission. Explicit opt-in defines one logical queue
whose documented bound is the sum of its fixed reservations. Its existing
`queue.size` is expressly the BE capacity under local scope, not an unchanged
aggregate limit. Reject zero/unlimited capacities and checked-arithmetic overflow.

If FE slots are released at acquisition, each front permits at most F queued
messages plus one active/retry holding of at most D messages, where
D=`min(queue.dequeueBatchSize,F)`. S2 must not allocate another active or retry
holding while that lease remains unresolved. Current BE admission uses physical
`iQueueSize`, including dequeued but unretired entries, so its accepted holdings
are already bounded by B. Therefore the conservative accepted-obligation bound is
`B + N*(F + D)`, not `B + N*F` alone. If the selected ring retains slots until
terminal retirement, use and report the tighter bound after review.

Also budget allocated ring/descriptor bytes, N FE worker stacks and execution
state, configured BE worker stacks and batches, bounded action transaction
arrays, and deferred-destruction arrays. Distinguish duplicate references from
additional delivery obligations. Raw-message sizes, dynamic message properties
and module buffers mean a slot bound alone is not a strict RSS byte guarantee.
For tests, use fixed payload/property sizes and record RSS. N is a lifetime FE
registration cap for S2; it does not silently grow when producers are replaced.

This partitioning intentionally allows a free FE to admit a fitting batch even
while BE admission is blocked. Conversely, a no-fit batch can wait in BE although
another producer's FE has room. These are the agreed private-front routing
semantics. A full FE causes no wait by itself. A failed FE capacity check consumes
no references; BE keeps its established per-element outcomes, including partial
batch admission. Never claim atomic BE acceptance from whole-tier routing.

### 4.2 Local wrapper reference outcomes

For the S2 FixedArray-only backend with sampling and severity discard disabled,
the local submission wrapper consumes every supplied reference exactly once,
including references rejected before admission. Successful FE publication transfers
all supplied references. After selecting BE, process the actual batch entries
through the existing per-message admission path. `RS_RET_OK` transfers the current
reference; `RS_RET_QUEUE_FULL` has consumed it by explicit discard. In this MVP,
`RS_RET_FORCE_TERM` leaves the current reference unconsumed: destroy that reference
and all unprocessed tail entries, recording pre-admission rejection. Do not retry
the consumed prefix or reinterpret a partial BE result as atomic rejection.
Unexpected errors require an explicit ownership outcome rather than guessing from
the return value. The FixedArray storage handler itself does not allocate or fail
on add. Enabling LinkedList or disk requires extending this outcome table first.
Global submission behavior is unchanged by this local-only wrapper contract.

Shutdown transfer differs from external submission: work is already accepted.
Track whether each reference was transferred, retained for another attempt, or
terminally discarded under the memory shutdown policy. A transfer failure is not
a second admission failure for that obligation. Bound waits by the remaining
logical deadline and retain the unprocessed suffix until accounted for.

### 4.3 Policy interpretation

S2 flow-control/full-delay/light-delay thresholds remain BE-specific and apply
when BE is selected; their units are BE physical messages and their defaults
continue to derive from B. This is an explicit local-scope interpretation, not a
claim that they are aggregate logical thresholds. FE admissions are bounded by
the separate reservation. Reject severity-based discard and queue sampling in
S2 because applying them only at BE would silently change configured message
selection. Later stages may introduce reviewed aggregate policies.

Current `doEnqSingleObj` increments `ctrEnqueued` before discard/admission;
`qqueueAdd` implements sampling; `qqueueChkDiscardMsg` runs at enqueue and dequeue.
Keep these global behaviors unchanged. For local scope, count external attempts
once before routing; successful FE publication or BE ownership acceptance counts
an admission once. Completion, configured terminal discard, outstanding work and
pre-admission failures reconcile separately. Tier transfer does not count as a
new attempt or admission. Maintain FE queued/active/retry and BE queued/active
views; active and retry must not double-count a retained lease. Scrapes may be
non-atomic snapshots; exact reconciliation is required at quiescent test barriers.

Prefer per-FE counters with one writer per counter and atomic snapshot loads,
aggregated for the logical queue at stats-read time, to avoid adding a contended
logical-counter increment to each FE message. BE fallback uses BE-owned counters;
registration and shutdown counters may use cold-path synchronization. The existing
`statsobj.SetReadNotifier` is a post-read notification, including in Prometheus
generation, so it cannot supply fresh pre-read aggregation without changing its
semantics. Add a separate narrow pre-read snapshot hook if needed. A snapshot
must not take the registry lock while holding stats-list locks if registration
can register stats objects under the registry lock. Fixed retained descriptors
allow lock-free bounded iteration of atomically published entries. Specify
counter reset semantics separately from ownership counters; an impstats reset
must not erase outstanding-delivery evidence.

Source anchors: [queue.c](../../../runtime/queue.c) `getLogicalQueueSize`,
`doEnqSingleObj`, `qqueueAdd`, `qqueueChkDiscardMsg`, `DeleteProcessedBatch`,
`qqueueMultiEnqObjNonDirect`, `qqueueSetDefaultsRulesetQueue`.
Stats source: [statsobj.c](../../../runtime/statsobj.c)
`generatePrometheusStats`, `getAllStatsLines`.

## 5. Exhaustive existing queue option matrix

All 35 entries in `runtime/queue.c` `cnfpdescr` at the recorded baseline appear
below. No existing option is FE-specific; the two proposed local bounds are new.
“Rejected” means a requested nontrivial feature is rejected for local scope;
ordinary inactive defaults do not themselves prohibit local scope. Preserve
explicit-used flags when the table rejects even an explicit inert disk option.
Validate inherited effective settings too. Global scope is unchanged.

| Existing parameter | Class | S2 rule |
|---|---|---|
| `queue.filename` | Rejected | Reject any configured prefix: would enable DA |
| `queue.spooldirectory` | Rejected | Reject explicit queue option; inherited global workDirectory is harmless without disk |
| `queue.size` | BE-specific | Positive bounded B; include it in the reported aggregate reservation |
| `queue.dequeuebatchsize` | Adapted | Positive BE ceiling, FE ceiling min(value,F); actual count may be smaller, no default accumulation |
| `queue.mindequeuebatchsize` | Rejected | Effective value must be 0 |
| `queue.mindequeuebatchsize.timeout` | Rejected | Reject explicit use; inactive inherited timeout harmless when minimum is 0 |
| `queue.maxdiskspace` | Rejected | Reject explicit use; inactive default does not enable disk |
| `queue.highwatermark` | Rejected | Reject explicit DA threshold; default inactive without DA |
| `queue.lowwatermark` | Rejected | Reject explicit DA threshold; default inactive without DA |
| `queue.fulldelaymark` | BE-specific | Existing BE flow-control behavior; never an FE or aggregate threshold |
| `queue.lightdelaymark` | BE-specific | Existing BE flow-control behavior; never an FE or aggregate threshold |
| `queue.discardmark` | Rejected | Reject explicit use while local severity discard is unsupported |
| `queue.discardseverity` | Rejected | Effective disabled value 8 only; no FE bypass of active severity discard |
| `queue.checkpointinterval` | Rejected | Reject explicit use; no persistence path |
| `queue.syncqueuefiles` | Rejected | Reject explicit use; no persistence path |
| `queue.type` | BE-specific | FixedArray initially; LinkedList only after qualification; Direct/Disk/SegmentedDisk rejected |
| `queue.diskqueuetype` | Rejected | Reject explicit use |
| `queue.diskqueueautoupgrade` | Rejected | Reject explicit use |
| `queue.diskqueueidletimeout` | Rejected | Reject explicit use |
| `queue.workerthreads` | BE-specific | Positive dedicated BE pool maximum; separate from N FE consumers |
| `queue.timeoutshutdown` | Logical/adapted | One logical graceful deadline covering FE plus BE, not one full allowance per FE |
| `queue.timeoutactioncompletion` | Logical/adapted | Coordinated action-completion phase including all workers and source leases |
| `queue.timeoutenqueue` | BE-specific | Existing BE admission timeout and reference outcomes; FE never waits for local space |
| `queue.timeoutworkerthreadshutdown` | Adapted | BE idle-worker retirement may retain existing meaning; FE consumer lifetime fixed until logical shutdown |
| `queue.workerthreadminimummessages` | BE-specific | Existing BE worker advice threshold only; never controls FE count |
| `queue.maxfilesize` | Rejected | Reject explicit use |
| `queue.saveonshutdown` | Rejected | Reject explicit on; off allowed. Inherited on is inactive without a filename, as today; no persistence claim |
| `queue.dequeueslowdown` | Rejected | Effective 0 only; use controlled callback cost for measurements |
| `queue.dequeuetimebegin` | Rejected | No explicit schedule; inactive effective defaults only |
| `queue.dequeuetimeend` | Rejected | Effective 25 (disabled); no explicit schedule |
| `queue.cry.provider` | Rejected | Reject explicit use |
| `queue.samplinginterval` | Rejected | Effective 0 only |
| `queue.takeflowctlfrommsg` | BE-specific | Existing BE semantics; ordinary main-queue submissions already pass each message's flowCtlType |
| `queue.mutexcontentionstats` | BE-specific | Existing BE mutex metrics only; distinguish FE wake/registry metrics |
| `queue.oncorruption` | Rejected | Reject explicit use; no disk corruption policy in memory MVP |

## 6. Shutdown and true blockers

`deinitAll` already sets global input termination, joins input threads, flushes
internal messages, destroys main Q, then destroys ruleset/action state. With S2's
no-call/no-asynchronous-boundary restriction, preserve this order and extend each
logical queue destructor to cover all its FEs before stopping its BE. Late
internal messages use BE while it remains available. Once routing closes, reject
new delivery with an explicit reference/terminal policy; do not leave a dangling
queue pointer in a TLS cache or silently lose an accepted FE obligation.

One logical shutdown budget must include waiting for routing quiescence, graceful
FE execution and BE draining. If expiration invokes cancellation, join all FE
workers before a maintenance thread touches their active lease or ring. Only
then may residual pending work transfer to BE. A full BE cannot cause a shutdown
deadlock: wait only within the remaining coordinated allowance, retain exact
ownership after partial failure, and apply the documented existing memory-only
terminal discard policy on expiration. “Safe shutdown” is ownership-safe bounded
cleanup; without DA/save it is not a new promise of lossless persistence after a
deadline. Successful graceful tests must nevertheless drain all accepted work.

No architectural or user-preference blocker was found in this bounded audit.
The following are implementation acceptance blockers, not decisions to leave
silently open:

- The companion lease design must establish exact reference ownership on BE
  errors, partial batches, FE cancellation and shutdown transfer before S2 code.
- omfile transactional and module-specific configuration qualification must pass
  before enabling that entry; otherwise retain only the controlled test callback
  and state that the production file pipeline is unqualified.
- The hard validator, source tagging, bounded resources, wakeup proof, and
  deterministic lifecycle oracles must exist before runtime activation is allowed.
- This draft does not satisfy stage acceptance: sanitizer, full applicable
  container validation, source review, batching evidence and predeclared paired
  performance gates remain required by the implementation plan.
