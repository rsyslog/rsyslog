.. _dev-main-overview:

Developer Overview: rsyslog Engine
==================================

.. page-summary-start

Comprehensive technical overview of rsyslog’s core engine, queue types, concurrency handling,
and worker thread model, aimed at contributors and advanced users.

.. page-summary-end

Overview
--------
rsyslog processes messages through a modular architecture built around queues, rulesets, and actions.
The design enables high throughput, flexible routing, and resilience under load or failure conditions.

Key Components
--------------
- **Inputs**: Modules that receive messages from various sources (e.g., imtcp, imudp).
- **Rulesets**: Contain filters and actions; can be invoked by inputs or other rulesets.
- **Actions**: Output or processing steps (e.g., omfile, omfwd).
- **Queues**: Decouple producers from consumers, manage flow control, and ensure durability.

Queue Types
-----------
- **direct**: No buffering; messages are handed directly to the action. Lowest latency but action must keep pace.
- **in-memory**: Stored in RAM (LinkedList or FixedArray); volatile but fast.
- **disk-assisted (DA)**: Combination of in-memory (primary) and on-disk spillover when memory fills.
- **disk-only**: Persistent on-disk storage; highest durability, higher latency.

Concurrency Model
-----------------
- Each action may have its own queue and multiple worker threads.
- Worker threads dequeue messages in batches and execute the action logic.
- Thread safety is enforced via locks or atomic operations where state is shared.
- Avoid blocking operations in action workers; prefer non-blocking I/O.

Concurrency Handling in Detail
------------------------------
- **Worker threads per action** are configured with ``queue.workerThreads``.
- **Batching** improves throughput but may introduce reordering between batches.
- **Synchronization primitives** (mutexes, RW locks) are used only when necessary for shared state in pData.
- **Atomic counters** and memory barriers ensure correctness without excessive locking.
- Actions must clearly signal transient vs. permanent failures to allow the queue engine to handle retries or discards appropriately.

Queue Flow (Mermaid Diagram)
----------------------------
.. mermaid::

   flowchart LR
     Inputs[Inputs] --> MainQ[Main Queue]
     MainQ --> Rules[Rulesets]
     Rules -->|Filter match| ActQ1[Action Queue 1]
     Rules -->|Filter match| ActQ2[Action Queue 2]
     ActQ1 --> W1[Workers 1]
     ActQ2 --> W2[Workers 2]
     W1 --> A1[Action 1]
     W2 --> A2[Action 2]

Backpressure & Flow Control
---------------------------
- **High-water mark**: When reached, producers block or slow down.
- **Low-water mark**: Normal operation resumes below this threshold.
- **Discard policy**: Drop lower-priority messages first when full.
- **Timeouts**: Control enqueue blocking and shutdown grace periods.

Error Handling
--------------
- **Transient errors**: Retries with exponential backoff.
- **Persistent errors**: Drop, dead-letter queue, or keep retrying based on configuration.
- **DA/disk queues**: Preserve messages across restarts.

Cross-References
----------------
- :doc:`dev_action_threads`
- :doc:`../concepts/queues`
- :doc:`../rainerscript/queue_parameters`

