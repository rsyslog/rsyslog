.. _dev-action-threads:

Action Threads and Queue Engine (Developer-Oriented)
====================================================

Overview
--------
The rsyslog queue engine connects producers (inputs/rulesets) with consumers (actions),
providing durability, concurrency, backpressure, and error handling. Each action may
use a dedicated queue (or direct mode) and one or more worker threads. The engine
balances throughput and ordering while coping with failures and shutdown.

User-facing queue concepts and parameters are covered in :doc:`../concepts/queues`
and :doc:`../rainerscript/queue_parameters`.

Terminology
-----------
- **Producer**: Component that enqueues a message (main queue, ruleset, or action caller).
- **Action**: Output/processing step (e.g., omfwd, omfile).
- **Action queue**: The queue attached to an action (or direct mode without an explicit queue).
- **Worker thread**: Thread that dequeues batches, executes the action, and finalizes results.
- **Batch**: Group of messages processed as a unit (see ``queue.dequeueBatchSize``).
- **Backpressure**: Throttling behavior when queue reaches high-water marks.
- **Retry/Suspend**: Temporary action pause with backoff when the action fails.
- **DA mode**: Disk-assisted in-memory queue (memory primary, spills to disk on pressure).
- **Disk-only queue**: Persistent queue on disk for maximum durability.

Thread and Queue Topology
-------------------------
- Main message queue feeds rulesets. Rulesets evaluate messages and dispatch to actions.
- For each action:
  - **direct** mode: No queue; messages are synchronously handed to the action.
  - **memory/disk/DA** queue: Messages enter an action queue processed by N workers.
- Worker threads scale per action (``queue.workerThreads``) and dequeue in batches.

.. mermaid::

   flowchart LR
     Inputs[Input modules] --> MainQ[Main Message Queue]
     MainQ --> Rules[Ruleset Processor]
     Rules --> A1Q[Action Queue A1]
     Rules --> A2Q[Action Queue A2]
     A1Q --> A1W[Workers A1]
     A2Q --> A2W[Workers A2]
     A1W --> A1Out[Action A1 (omfile)]
     A2W --> A2Out[Action A2 (omfwd)]

Message Processing Lifecycle (Action Queue)
-------------------------------------------
1. Enqueue
   - Producer pushes message to the action queue.
   - DA/disk queues persist as configured; memory queues hold in RAM.
2. Worker activation
   - Idle worker threads wake when items are available.
   - Thread count respects ``queue.workerThreads`` (and internal min/max).
3. Dequeue and batch
   - Worker dequeues up to ``queue.dequeueBatchSize`` messages (not less than ``queue.minDequeueBatchSize`` unless draining or shutting down).
   - Per-queue FIFO ordering is preserved; inter-queue ordering is not guaranteed.
4. Execute action
   - Worker invokes the action module once per batch or per message depending on the action.
   - Action may signal success, transient failure (retry), or fatal failure (discard or suspend).
5. Commit/finalize
   - On success, messages are acknowledged and removed (and persisted offsets advanced).
   - On transient failure, worker applies backoff/suspend logic; messages remain pending.
   - On fatal failure, behavior follows queue/action discard policy.
6. Idle/scale-down
   - If the queue is drained, workers may sleep and be reclaimed after timeouts.
7. Shutdown
   - On shutdown, workers attempt to drain within ``queue.timeoutShutdown`` and honor persistence settings (e.g., ``queue.saveOnShutdown`` for DA/disk).

Backpressure and Flow Control
-----------------------------
- **Watermarks**:
  - High-water mark: When reached, producers slow down or block per timeout settings.
  - Low-water mark: Normal operation resumes below this threshold.
- **Discard policy**:
  - When near/at capacity, use ``discardMark`` and ``discardSeverity`` to shed lower-priority messages first (if configured).
- **Timeouts**:
  - ``queue.timeoutEnqueue``: Producer blocking time when queue is full.
  - ``queue.timeoutActionCompletion``: How long a worker waits for the action to complete.
- **Rate limiting**:
  - Configurable in some actions to avoid overloading downstreams.

.. mermaid::

   stateDiagram-v2
     [*] --> Empty
     Empty --> Filling: enqueue
     Filling --> High: size >= highWatermark
     High --> Draining: dequeue/batches
     Draining --> Filling: enqueue > dequeue
     Draining --> Empty: size == 0
     High --> Filling: size < highWatermark

Error Handling, Retry, Suspend
------------------------------
- Transient errors trigger backoff:
  - Worker suspends the action for a short interval and retries later (interval may grow).
- Persistent errors:
  - Depending on module and settings, move to dead-letter semantics, drop, or keep retrying.
- Disk-backed safety:
  - DA and disk queues keep messages across process restarts (subject to sync and checkpoint settings).

Queue Types and Selection
-------------------------
- **direct**: Lowest latency, no buffering; action must keep up or it becomes a bottleneck.
- **in-memory**: High throughput, volatile; messages lost on crash unless DA or disk-backed.
- **disk-assisted in-memory (DA)**: Fast under normal load, durable under bursts.
- **disk-only**: Highest durability, higher latency; best for critical delivery.

Key Parameters (see :doc:`../rainerscript/queue_parameters`)
------------------------------------------------------------
- ``queue.type``: direct, LinkedList (memory), FixedArray (memory), DA, disk.
- ``queue.size``: Capacity in number of messages (memory queues).
- ``queue.dequeueBatchSize`` / ``queue.minDequeueBatchSize``: Batch sizing.
- ``queue.workerThreads``: Max concurrent workers per action.
- ``queue.highWatermark`` / ``queue.lowWatermark``: Backpressure thresholds.
- ``discardMark`` / ``discardSeverity``: Controlled shedding under pressure.
- ``queue.spoolDirectory`` / ``queue.filename``: Disk storage for DA/disk queues.
- ``queue.checkpointInterval`` / ``syncQueueFiles``: Durability and fsync policy.
- ``queue.timeoutEnqueue`` / ``queue.timeoutShutdown`` / ``queue.timeoutActionCompletion``: Timing behavior.
- ``queue.saveOnShutdown``: Persist pending entries at shutdown (DA/disk).

Sequence and Error Paths
------------------------
.. mermaid::

   sequenceDiagram
     actor P as Producer
     participant Q as Action Queue
     participant W as Worker Thread
     participant A as Action Module

     P->>Q: enqueue(msg)
     Q-->>W: wake
     W->>Q: dequeue(batch)
     W->>A: process(batch)
     A-->>W: success | transient_error | fatal_error

     alt success
       W->>Q: commit/remove(batch)
     else transient_error
       W->>W: backoff/suspend (retry later)
     else fatal_error
       W->>Q: discard or DLQ policy
     end

     W-->>Q: next batch or sleep

Developer Notes
---------------
- Batching improves throughput but increases per-message latency; tune batch sizes per action characteristics.
- Parallel workers can reorder across batches; per-queue FIFO is preserved, but global ordering is not.
- Avoid blocking in action code; prefer non-blocking I/O and internal buffering where possible.
- Ensure action modules clearly communicate transient vs. permanent errors to the engine.

Cross-References
----------------
- :doc:`../concepts/queues`
- :doc:`../rainerscript/queue_parameters`
- :doc:`../whitepapers/queues_analogy`