.. _transactional-config-reload-adr:

.. meta::
   :description: ADR for transactional configuration reloads, generation ownership, and zero-disruption input preservation in rsyslog.
   :keywords: rsyslog, ADR, configuration reload, generation, ruleset, imtcp, transactional, performance

Transactional configuration reload
==================================

.. summary-start

This ADR defines the staged transactional-reload contract: private validation
and capability reporting precede activation; live changes commit atomically;
and retiring work drains only after consumers quiesce at a batch boundary.

.. summary-end

Status
------

Accepted as the implementation contract for the staged transactional-reload
program.  This is a developer design record, not a statement that every stage
is already available in a released rsyslog version.

Context
-------

Configuration reload must not expose a partly constructed configuration to a
message-processing thread, stop an established TCP session solely because an
unrelated ruleset changed, or make the normal message path pay for reload
coordination.  Existing pointers from a message to its ruleset runtime are
especially sensitive: a per-message lock, generation lookup, or reference
count in that path would turn a rare control-plane event into a permanent data
plane cost.

The design must also account for objects with external effects.  Actions and
templates can own queues, files, network connections, or worker state; modules
can expose only a subset of lifecycle operations; and an ``imtcp`` listener can
have both a listening socket and many live sessions.  A reload that fails while
creating any of these objects must leave the active configuration untouched.

HUP policy and legacy hooks
---------------------------

The global setting ``global(config.reloadOnHUP="off|validate|on")`` controls
what a HUP requests.  Its default is ``off`` to preserve existing behaviour.

``off``
  Run the established legacy HUP path and its legacy module hooks.  No
  transactional candidate is constructed.

``validate``
  Construct and privately validate a shadow candidate and emit its diff and
  capability report.  It never prepares live resources or activates a new
  generation; the HUP then follows the legacy-hook path.

``on``
  Construct, validate, report, and, once the applicable later delivery stages
  exist, execute the transactional path.  Legacy hooks remain invoked in their
  established order at the HUP integration boundary; they are not candidate
  preparation hooks and may not mutate a private candidate or the active
  generation during validation.

The acceptance order is fixed: select the HUP mode, parse a private shadow,
validate it, calculate and publish the diff/capability report, reject if any
required capability or resource limit is absent, prepare the enabled live
objects, commit one generation switch, invoke the applicable legacy boundary
hooks, and retire old state after quiescence.  A stage that does not implement
one of these operations stops at its documented boundary and must not perform
a later operation implicitly.

Configuration frontend parity
-----------------------------

Reload semantics are defined on the normalized configuration object graph,
not on RainerScript syntax.  RainerScript, native YAML, and YAML configurations
containing embedded ``script:`` blocks must therefore produce the same diff,
capability decision, cutover boundary, diagnostics, and rollback result.
Every delivery stage has equivalent end-to-end reload tests for RainerScript
and YAML; mixed YAML/script coverage is added wherever the affected construct
can cross that frontend boundary.  Parser acceptance tests alone are not
sufficient evidence of reload parity.

Decision
--------

Reload is a transaction over immutable configuration generations.  The reload
controller builds a private candidate generation and advances it through the
following lifecycle:

``draft``
  Parse the requested configuration and construct only private objects.  A
  draft has no publication path to message consumers.

``validated``
  Complete semantic validation, including cross-object references, ruleset
  linkage, configuration frontend parity, and module capability checks.  A
  validation error discards the candidate.

``prepared``
  Allocate and initialize all activation-time resources that can be prepared
  safely: action and template state, queues, module instances, and reusable
  input resources.  Preparation must either succeed completely or roll back
  every resource acquired by the candidate.

``active``
  Atomically publish the prepared generation as the sole generation selected
  for new work.  Publication is one ordered pointer/generation switch; it is
  not a sequence of visible per-object substitutions.

``retiring``
  The previously active generation remains valid for work already assigned to
  it.  It is destroyed only after every relevant consumer has acknowledged a
  quiescent batch boundary and all retained runtime references are gone.

The controller may move only forward through these states.  A failed candidate
never becomes active.  Retiring is not rollback: once activation succeeds, a
later cleanup failure is reported and retried or contained without resurrecting
the old generation as active.

Atomic failure semantics
------------------------

Before activation, failure has all-or-nothing meaning.  Parse, validation,
preparation, resource-allocation, and capability failures must leave the active
generation, its public object graph, and its inputs exactly usable as before
the reload began.  Candidate cleanup must not close, flush, detach, or mutate
an active resource shared by identity with the candidate.

Activation has a single commit point.  Once the publication switch completes,
newly admitted work uses the new generation and old work completes against the
old one.  Errors after that point are retirement errors, not grounds for a
partial reverse switch.  The reload result and counters must distinguish
``rejected before activation``, ``activated with retirement pending``, and
``retirement failed`` so operators do not infer an incorrect active state.

Rulesets and the message hot path
---------------------------------

Ruleset runtimes use a pointer-stable shell while messages can refer to them.
The direct ``smsg_t`` pointer to that shell remains the normal
dispatch path: processing a message must not require a global generation lock,
hash lookup, indirection through a mutable ruleset table, or reload-time
reference-count operation.

At activation, the shell's private immutable execution plan is swapped at the
consumer barrier; the shell itself is never replaced while a message or session
can hold its ``smsg_t`` pointer.  The old plan remains owned by the retiring
generation until its assigned consumers quiesce.  Pointer stability therefore
protects in-flight messages without making an old plan appear in the active
generation's namespace.

Consumer quiescence is batch-boundary based.  A consumer records the generation
used for its current batch, processes that batch without a mid-batch switch,
then acknowledges the publication epoch before taking its next batch.  The
retirement coordinator waits for acknowledgements from every consumer that
could have received old-generation work.  This makes the boundary explicit and
keeps generation checks out of the inner per-message loop.

Actions, templates, and module capabilities
--------------------------------------------

Actions and templates belong to a generation unless they are explicitly
reusable by a compatibility contract.  Reuse requires equivalence of the
effective definition and compatible runtime ownership, including queue,
worker, and template dependencies.  Otherwise the candidate receives fresh
state and the prior state retires with the old generation.  No action or
template may be destroyed while old-generation work can still invoke it.

An action must have a stable explicit name before a changed definition is
eligible for lifecycle matching, replacement, retirement, or observability.
Unnamed actions are not silently matched by position or generated identity:
an unchanged unnamed action may retain its existing runtime, but changing one
rejects the candidate with a diagnostic that requests ``name=``.  An unchanged
object whose module does not advertise reload capability likewise remains
attached to the active generation unchanged; that is safe because no in-place
change is requested.  In contrast, a changed object with an unsupported reuse
or lifecycle capability atomically rejects the candidate before commit.  It
must never be partially rebuilt or mutated in place.

Modules declare the reload capabilities they support.  The capability contract
must say whether an instance can be validated privately, prepared before
publication, reused in place, activated atomically, and retired asynchronously.
The controller may reuse a module instance only when the module advertises the
needed capability and the effective input/action configuration is compatible.
For an unsupported change, policy is to reject the candidate before activation
or require the established full-restart path; it is never acceptable to perform
an unadvertised in-place mutation of an active module.

``imtcp`` preservation
----------------------

For a compatible ``imtcp`` input, the listener and its accepted sessions are
preserved across reload.  Compatibility includes the effective listener
endpoint tuple and transport/security parameters that determine the socket and
session protocol behavior.  Endpoint reconciliation uses the complete endpoint
tuple, not merely a port.  A ruleset or downstream action change alone must not
close the listener or its active sessions.

When an endpoint is removed, it stops accepting new sessions but its existing
sessions remain alive.  For those sessions, TLS, framing, and compression are
frozen for the lifetime of the session.  A compatible ruleset update changes a
session's ruleset-shell pointer through an event-loop control event at the
safepoint, namely before the next complete message is processed.  An ACL change
is evaluated for the next message and may drop that message without
disconnecting the session.  Removing a ruleset still bound to a preserved
session rejects the candidate; no fallback rebinding is permitted.

If a listener is incompatible, the candidate must prepare the replacement
without disturbing the active listener; inability to bind or prepare the
replacement rejects the transaction before activation.  A deliberate handoff
policy for the same endpoint must be explicit and tested, because it cannot be
assumed from generic module reuse.

Tombstones and removed objects
------------------------------

Removing a named ruleset, action, template, or input from the new configuration
removes it from the active-generation lookup namespace immediately at commit.
The old runtime is retained as a tombstone only in the retiring generation.
Tombstones provide enough identity and diagnostic context for old work to
finish or report a controlled error; they must not route newly admitted work to
the removed object or silently rebind it to an unrelated replacement.  They are
reclaimed with the retiring generation after quiescence.  Tombstones are bounded
by a configured implementation limit.  A candidate that would exceed that
limit is rejected before commit, rather than allowing unbounded deferred state.

Observability and operator contract
-----------------------------------

The completed multi-release reload controller must expose the active generation
identifier, candidate state, last rejection reason, activation time, retirement
backlog, and the following target monotonic counters: ``reload_hup_off_total``,
``reload_validate_total``, ``reload_validate_rejected_total``,
``reload_capability_rejected_total``, ``reload_tombstone_limit_rejected_total``,
``reload_prepare_total``, ``reload_prepare_failed_total``,
``reload_activated_total``, ``reload_retirement_pending_total``,
``reload_retired_total``, ``reload_retirement_failed_total``,
``reload_listener_preserved_total``, ``reload_listener_replaced_total``,
``reload_session_preserved_total``, ``reload_acl_message_dropped_total``, and
``reload_legacy_hook_total``.  It must log the generation IDs, diff/capability
result, and commit outcome at an operator-visible level.  Metrics and debug
diagnostics must allow a maintainer to answer: which generation is active, why
a candidate was rejected, which consumers still prevent retirement, and whether
listener, session, or module state was preserved or replaced.

The Release B foundation additionally exposes request-, mode-, rejection-, and
duration-accounting counters.  Its first implementation parses RainerScript and
YAML candidates into owned source-syntax objects without invoking module
constructors, global setters, queue construction, or input activation.  It
compares that graph with a mirror captured during the original startup parse and
reports a successful comparison as ``reported_only``.  This result is a
conservative source-syntactic diff, not semantic validation: effective defaults,
module capability classification, preparation, and ``on`` activation remain
fail-closed until their later gates are complete.  Later releases retain or
refine these counters alongside the target outcome counters above.

Modules may opt in to a private source-lowering interface as those later gates
are developed.  Both ``validate`` and ``on`` then lower the active and candidate
source catalogs through the module's ordinary parameter and default logic.  A
lowerer may reject a syntactically captured but semantically invalid setting,
but it must not construct runtime resources, change the active generation, or
retain borrowed catalog data.  In ``validate`` mode this remains report-only;
successful lowering is not permission to activate the module.
The status field ``source_capability`` distinguishes an exact effective
``reuse`` comparison from a conservative ``restart_required`` result and from
``not_evaluated`` when parsing, lowering, legacy syntax, or an unsupported
side-effectful setting prevented comparison.
The first imtcp comparator pairs listener instances in source order; a pure
reorder is therefore conservatively restart-required until endpoint-key
reconciliation is connected to the runtime registry.

Release C extends that foundation with a deliberately narrow private compiler
and batch-boundary activation path.  In ``on`` mode, only modifications to
already existing rulesets can be prepared and atomically activated.  Added or
removed rulesets and every change to actions, inputs, parsers, queues,
templates, modules, or global settings remain rejected as
``candidate_scope_unsupported``.  Unsupported ruleset syntax and consumer
queues without a safe batch barrier are rejected before commit.  This is the
first Release C scope; it does not imply that other configuration objects are
live-reloadable.

Diagnostics must avoid dumping message contents, credentials, or TLS material.
Generation identifiers are operational correlation values, not a substitute
for configuration provenance or audit logging.

Delivery plan and gates
-----------------------

The program is staged so that invariants become testable before broad module
reuse is enabled.

Release A
  Publish this ADR and establish the terminology, state machine, atomicity
  contract, and required test/performance gates.  No runtime behavior changes
  are implied.

Release B
  Introduce only private shadow construction and validation plus a deterministic
  diff/capability report.  It performs neither live preparation nor activation.

Release C
  Add live pointer-stable ruleset shells whose private plans may change only for
  filters, expressions, branches, and calls.  Parsers, queues, actions, and
  input bindings remain unchanged in this stage.  The private plan swap occurs
  at the consumer barrier.

Release D
  Add named actions and templates, their lifecycle compatibility contracts, and
  queue drain/retirement.  This stage includes negative tests for unsupported
  changed reuse and the unchanged-unsupported preservation rule.

Release E
  Add endpoint-tuple reconciliation for ``imtcp``.  A removed listener stops
  accepting only; established sessions preserve frozen TLS, framing, and
  compression.  Session ruleset-shell updates use the event-loop control event
  at the safepoint/next complete message, ACL changes drop the next message
  without disconnecting, and bound-ruleset removal rejects the reload.

Release F
  Expand the capability matrix and supported object set, complete hardening and
  observability, and add release-level regression coverage across supported
  configuration frontends.

Each functional stage must have deterministic tests covering successful
activation, every pre-commit failure class, old-work completion, object removal,
and resource cleanup.  Stages that introduce reuse must test both compatible
preservation and incompatible replacement.  ``imtcp`` functional tests keep a
TCP connection open while numbered messages are sent before, during, and after
HUP.  They must prove the stated event-loop boundary and preservation,
ACL/drop, exact-delivery, and ordering behaviour without relying on sleeps.

The mandatory performance gate runs *after focused tests, rollback tests,
ASAN/UBSAN, lifecycle TSAN, static analysis, and PR-ready container validation
all pass*.  Baseline and candidate use separate worktrees and identical builds.
After one unscored calibration pair, each session contains at least eleven
alternating measured pairs; the entire campaign is repeated in a second,
independent and counterbalanced session.  The matrix covers 128-, 512-, and
4096-byte messages, one and many TCP sessions, default and named rulesets,
direct and asynchronous queues, and meaningful batch sizes from 1 through
1024.  Steady-state and reload-transient results are reported separately.

The gate records throughput, CPU per message, latency percentiles, cycles,
instructions, branches and misses, cache misses, context switches, syscalls,
allocations, and lock contention.  Sampling profiles and, for central fast-path
changes, disassembly comparisons must show that no new message-proportional
work was introduced.  In particular, transactional reload must not add a
per-message name lookup, allocation, lock, reference-count operation, or
syscall.  Every reproducible deterioration rejects the candidate.  A noisy or
otherwise inconclusive comparison is rerun under more stable conditions and is
not a pass.  Each stage archives a versioned report with revisions, workloads,
raw metrics, dispersion, commit pause, and the accept/reject decision.  A
functional pass without this performance result is not release-ready for a
stage that changes the data path.

Consequences
------------

This design makes reload behavior easier to reason about: candidates are
private until one commit point, old work has a bounded ownership home, and
retirement is observable rather than an implicit race.  It also imposes strict
implementation discipline.  Lifecycle ownership and module capability metadata
become part of the internal interface, and tests must model concurrent
admission, batches, and cleanup rather than only configuration parsing.

The primary trade-off is temporary duplication of generation-owned resources
and delayed reclamation while consumers drain.  That cost is accepted to keep
reload coordination out of the steady-state ``smsg_t`` dispatch path and to
preserve compatible TCP service continuity.
