.. SPDX-License-Identifier: Apache-2.0

.. _dev_config_reload:

Config Reload on HUP: Design Record
===================================

.. meta::
   :description: Developer design record for applying rsyslog configuration changes on SIGHUP: requirements, invariants, chosen architecture, milestones, restart-only settings and the OFF-mode compatibility gate.
   :keywords: rsyslog, HUP, SIGHUP, config reload, reloadOnHUP, rsconf, ruleset, msgConsumer, design record, ADR

.. summary-start

Design record for config reload on HUP. A HUP first validates the changed
config in a child ``rsyslogd -N1``. Later it loads a never-activated donor
config in process and patches the running config at a per-batch gate in
``msgConsumer()``. Ruleset bodies come first, listeners and settings later.
Everything is off by default.

.. summary-end


Status
------

Accepted design, implementation in progress. Tracking issue:
`#8007 <https://github.com/rsyslog/rsyslog/issues/8007>`_.

This page records requirements, invariants and decisions so that reviewers
and later contributors can check each pull request against them. It is not
user documentation; the user-facing setting is documented with the release
that ships it. Function names are cited instead of line numbers because line
numbers go stale.

Nothing described here is user visible yet. Until a milestone ships, a HUP
does exactly what it did before: it runs the legacy HUP hooks (file reopen,
lookup table reload, and similar) and nothing else.


Goal and requirements
---------------------

End goal: a HUP applies configuration changes while established sessions,
in particular TCP sessions, stay up, as far as the change permits.

Requirements for every step on the way:

- **No throughput loss.** Ruleset execution is the hot path. The design must
  not add per-message locks, lookups, reference counts or atomics. From the
  milestone that touches the data path on, a benchmark against ``main`` is an
  acceptance criterion.
- **Fail safe.** A change that cannot be applied leaves the running config
  untouched and is reported with a reason that names the object and, where
  known, the source location. The operator then fixes the config or restarts.
- **No** ``exit()`` on any path reachable after startup.
- **Both frontends.** RainerScript and YAML (including YAML with ``script:``
  blocks) yield the same decisions. Every reload test has a YAML twin.
- **Tests run in parallel** (``make check -j``) without relying on timing.
- **Small, shippable steps.** Each pull request delivers something usable or
  is a standalone fix. No slice may depend on a later slice to be correct.
- **OFF equals main.** With the feature off, behaviour and output are the
  same as before the feature existed (see :ref:`dev_config_reload_off_gate`).
- New source files carry an SPDX Apache-2.0 header.


User-visible setting
--------------------

.. code-block:: none

   global(config.reloadOnHUP="off|report|on")

- ``off`` (default): today's HUP behaviour.
- ``report``: on HUP, if the config files changed, validate them and log one
  outcome line. Nothing is applied.
- ``on``: apply the supported part of a change; reject the whole change, with
  the reason, if any part is unsupported.

The legacy HUP hooks always run first and unchanged, in every mode. A reload
attempt only happens when the files read at load time changed.
``report`` and ``on`` stay documented as experimental until live reload has
soaked in production use.


Invariants
----------

These hold for every milestone. A pull request that needs to break one has to
change this page first.

1. **Single commit point.** A reload either changes nothing or commits at
   exactly one point. Failure before commit leaves the running config as it
   was. Problems after commit are retirement errors: they are logged and
   counted, never undone by switching back.
2. **The running** ``rsconf_t`` **and every** ``ruleset_t`` **stay alive.**
   They are patched in place. ``runConf`` is never replaced at runtime, so the
   many unsynchronised readers of ``runConf`` stay valid.
3. **The candidate config is a donor.** It is loaded detached, compared with
   the running config, and the changed objects are moved out of it. It is
   never passed to ``activate()``, so the missing rollback in ``activate()``
   and the privileged pre-drop hooks are not reload problems.
4. **Messages hold a stable pointer.** ``smsg_t`` keeps its direct
   ``ruleset_t *``. Only the contents of the ruleset (its body) change, at a
   batch boundary.
5. **Swaps happen only at the gate.** Ruleset code is entered in exactly one
   place, ``msgConsumer()`` in ``tools/rsyslogd.c``, for main, ruleset,
   disk-assisted and direct-mode queues. A swap happens only while every
   thread is outside that function. Action queues never run ruleset code and
   are never gated.
6. **Candidate objects are invisible.** A candidate must not register
   statistics, open outputs, bind sockets or start threads before it is
   committed.
7. **References resolve to running objects.** When the candidate defines an
   object (template, lookup table, dynstats or percentile bucket, ruleset)
   equal to a running one, name lookups during the candidate load return the
   running object. An adopted tree therefore never points into the donor.
8. **Unchanged objects keep their identity.** Reused actions keep their
   object, queue, statistics and action number. Action numbers are never
   reused within one process lifetime.
9. **Every change is classified.** A change the current milestone does not
   support rejects the whole reload, with the reason. Nothing is applied
   partially.


Architecture
------------

Reload runs on the main thread, in the main loop, after ``doHUP()``. Shutdown
and classic HUP also run there, so they can delay a reload but never overlap
with it. A termination request aborts a pending reload.

Steps of one reload attempt (``on`` mode, full form):

1. **Change check.** Compare the recorded config file list (files opened by
   the RainerScript lexer and the YAML loader, include globs and their
   expansions, with stat data and a content hash) against the files on disk.
   No change means no further work.
2. **Pre-flight.** Run the same binary as a child with the original config
   arguments and ``-N1``, with a timeout, without blocking the main loop. The
   child inherits the dropped privileges and the chroot, so it sees exactly
   what an in-process load can read. An invalid config stops here.
3. **Donor load.** Load the candidate in process through the normal loader,
   which must be repeatable for this (milestone M2).
4. **Classify** each difference against the reload-class table. Anything out
   of scope rejects.
5. **Prepare** everything the swap needs: allocations, action renumbering,
   new action queues started (still empty).
6. **Gate.** Request the gate and wait for every gated thread to be outside
   ``msgConsumer()``, bounded by a quiesce timeout.
7. **Swap.** Pointer stores only, no allocation, no I/O. Cannot fail.
8. **Release** the gate.
9. **Retire** old bodies and the rest of the donor.
10. **Report** one log line and update counters and the generation number.

Gate sketch (milestone M3): a global word holds a "reload pending" bit and a
config epoch, written only by the main thread. Each ``wti_t`` publishes
whether it is inside ``msgConsumer()``, with a nesting depth so that a nested
entry (``call`` into a direct-mode ruleset queue) never parks. Entry and the
reloader's request form a store-then-load pair with full barriers on both
sides. Per batch this costs a few stores to the worker's own ``wti_t`` and one
load of a read-mostly word. In direct mode a batch is one message, so the
cost there is per message. All ``wti_t`` objects are registered, so idle
workers and workers being started or stopped are covered. Worker cancellation
cleanup resets the gate state.

Worker action tables grow at the gate: each ``wti_t`` records the size of its
``actWrkrInfo`` array, and a thread resizes its own table when it sees a new
epoch. Every scan of the table is bounded by that size, which is already the
case on ``main``.


Decisions and rejected alternatives
-----------------------------------

Chosen:

- **Normal loader, used as a donor**, over a private parse-and-materialise
  pipeline. The loader is streaming (``cnfDoObj()`` and action construction
  run during the parse), so a private pipeline has to re-implement object
  construction. That second implementation would be a permanent maintenance
  cost.
- **Patch the running config in place** over config generations or new
  handle types. ``ruleset_t`` already is the pointer-stable shell.
- **Child-process pre-flight first.** It gives real validation with no loader
  changes and no data-path risk, and it stays useful as crash isolation in
  front of the in-process load.
- **Module opt-in for repeatable load** through the existing
  ``isCompatibleWithFeature()`` query. Modules without the flag make a
  config with them restart-only. No module interface version bump.
- **Processing graph before listeners.** Replacing ruleset bodies already
  lets TCP sessions survive the most common edits, because sessions route to
  ``ruleset_t`` shells that stay. imtcp listener changes follow per instance;
  "stop and start the whole module" is not an acceptable intermediate step.
- **Capture on top of** ``runtime/translate.c`` for canonical per-object
  descriptions and the diff. Legacy ``$`` directives are captured verbatim
  and in order.

Rejected (see the closed pull requests listed in #8007 for the code):

- Stopping and starting inputs in process: loses TCP sessions.
- Full ``Load()`` plus ``activate()`` of a new config on HUP: ``activate()``
  switches ``runConf`` before several fallible steps without rollback, and
  privileged initialisation cannot be repeated after privilege drop.
- Listener file-descriptor hand-off or ``SO_REUSEPORT``: old sessions keep the
  old config.
- A private syntax parse with a separate normalised graph and materialiser
  (the 2026 Codex prototype stack): second object-construction path, large
  shared-path changes, OFF-mode regressions.
- Two live versions of each ruleset body with per-batch selection (no
  parking): never stalls, but every other shared structure would need its own
  concurrent update, and the per-message path gains an indexed load. Revisit
  only if the bounded stall proves unacceptable.


Milestones
----------

M0 - groundwork (no behaviour change)
   This record; config destructors act on the config they are given; worker
   action tables bounded by their own size; grammar ``%destructor`` entries
   and lexer ownership so failed parses do not leak; exact HUP completion in
   the testbench (``GetHUPCount``/``AwaitHUPCount``); testbench helpers that
   report failures.

M1 - ``report`` mode
   File record and change check at startup and on HUP; pre-flight child with
   timeout, output capture and coalescing of HUP bursts; one outcome line
   (unchanged, invalid with first error, valid and changed); statistics
   counters. Ships as experimental.

M2 - repeatable in-process load and classification
   One-time initialisation separated from per-load work; legacy directives
   resolve through the config being loaded; side effects and statistics
   registration deferred to activation; failed candidates freed; ``exit()``
   in load paths turned into error returns (startup keeps today's exit codes);
   repeatable-load module flag; reference resolution to running objects;
   canonical descriptions and diff; ``report`` becomes object-level.

M3 - swap primitives, inert when off
   Per-batch gate, ``wti_t`` registry, ``call`` resolving through the ruleset
   instead of a cached body pointer, growable action tables, stable action
   identity and numbers, an imdiag command that swaps a body with an
   identical one in a loop. Benchmark against ``main`` is the exit criterion.

M4 - first live reload: ruleset bodies
   M4a: changed bodies of existing rulesets (default ruleset included),
   unchanged actions reused, new actions with direct or in-memory queues, new
   templates. M4b: removed and changed actions, changed templates, added and
   removed rulesets (a removed ruleset's shell routes to the default ruleset
   and counts).

M5 - imtcp listeners, then other inputs
   Unchanged instances and their sessions untouched; add, remove (with drain
   deadline) and rebind instances. Then a restartable flag for other inputs.

M6 - base settings
   A reviewed subset of ``global()`` parameters, TLS certificate reload for
   new sessions, queue tunables, lookup, dynstats, percentile and parser-chain
   definitions.

M7 - widen and harden
   Disk and disk-assisted queue changes, systemd ``Type=notify-reload``,
   generated reload-class documentation, soak testing.


Reload-class table
------------------

Every config object and setting gets exactly one class. The table is filled
in as milestones land; until an entry says otherwise, the class is
*restart*.

.. list-table::
   :header-rows: 1
   :widths: 30 15 55

   * - Object or setting
     - Class
     - Notes
   * - Ruleset body (filters, statements)
     - live (M4a)
     - Swapped at the gate; sessions and queued messages keep their ruleset
   * - Unchanged action
     - reused (M4a)
     - Keeps object, queue, counters and number
   * - New action, direct or in-memory queue
     - live (M4a)
     - Constructed and started before commit
   * - New template
     - live (M4a)
     -
   * - Removed or changed action, changed template
     - live (M4b)
     - Drained with the configured shutdown timeouts
   * - Added or removed ruleset
     - live (M4b)
     - Removed ruleset becomes a tombstone routing to the default ruleset
   * - imtcp instance added, removed or rebound
     - live (M5)
     - Other instances and their sessions untouched
   * - Selected ``global()`` parameters, TLS certificates
     - live (M6)
     - Per parameter, after review
   * - Everything else
     - restart
     - See the restart-only list


Restart-only list
-----------------

These stay restart-only until a milestone explicitly moves them:

- privilege, capability, chroot and umask settings;
- ``workDirectory``, ``maxMessageSize``, ``debug.*``, ``environment``,
  ``stdlog.channelspec``, ``operatingStateFile``;
- main queue type and file name;
- ruleset queue parameters and parser chains (input-side parsing in
  ``runtime/ratelimit.c`` runs outside the gate) until M6;
- changed actions with disk-backed queues (a new queue on the same spool file
  needs stop-old-before-start-new) until M7;
- experimental local queues;
- configs that use a module without the repeatable-load flag;
- inputs without the restartable flag;
- module unload;
- anything given on the command line.


.. _dev_config_reload_off_gate:

OFF-mode gate
-------------

Every pull request of this effort must show that ``off`` behaves like
``main``:

- ``-N1`` output identical to ``main`` over the test configs.
- Existing HUP tests unchanged and green.
- From M3 on, a throughput comparison against ``main``.
- Shared-path changes are reviewed against the list of regressions that the
  earlier prototype introduced in OFF mode:

  - one listener bind failure stopping all imtcp listeners; changed tcpsrv
    shutdown semantics; per-connection ``regcomp``; periodic ``poll()``
    wake-ups;
  - an unknown legacy property aborting the whole parse; optional include I/O
    errors becoming fatal; new error or info lines at every startup or HUP;
    ``$ResetConfigVariables`` dropping ``global()`` or ``main_queue()``
    settings; lost ``allowedSender`` diagnostics; the whole config cloned and
    hashed even when off;
  - ``SIGTERM`` removed from the blocked signal set (lost wake-up); dequeue
    time window ignored at shutdown; extra mutex pairs per message.


Testing expectations
--------------------

- RainerScript and YAML twins for every reload test.
- HUP tests wait with ``issue_HUP`` (exact completion count), never with
  sleeps.
- Reload paths run under ASan, UBSan and TSan lanes; the M2 load/destroy loop
  and the M3 swap loop run under ASan and TSan.
- Live reload tests (M4 on) check message conservation under load, repeated
  reloads with file descriptor, thread and memory checks, invalid candidates
  under load, suspended actions, reload racing HUP and shutdown, and an
  established imtcp session that survives and uses the new body.


Open points
-----------

Recorded defaults apply until decided otherwise:

- Quiesce timeout: 2 s with up to 3 retries before a reload is reported as
  not applied.
- Direct-mode main queues: supported through the same gate; decided after the
  TSan lane exists.
- Performance budget for M3: within run-to-run noise and at most 1 % on the
  imtcp-to-omfile benchmark, measured on quiet hardware.
- Platforms: ``report`` wherever spawning a child works; ``on`` developed and
  gated on Linux, refused with a clear message where untested.
