Action and Worker Thread Model
==============================

.. note::
   This document is derived from the current rsyslog source code. Some
   earlier developer notes (e.g. ``dev_oplugins.rst``) describe an
   older execution model and should not be relied upon without checking
   against the code.

Worker thread pool (wtp)
------------------------

``wtp_t`` represents a queue's worker thread pool. It tracks the pool
state, the number of running threads and callbacks supplied by the
queue:

- ``wtpState`` and ``iNumWorkerThreads`` control overall lifecycle and
  parallelism of the pool.
- ``pConsumer`` is the function executed by worker threads for each
  dequeued batch.
- ``mutWtp`` and ``condThrdInitDone``/``condThrdTrm`` serialize worker
  management.

These fields are defined in ``runtime/wtp.h``【F:runtime/wtp.h†L47-L75】.

Each queue owns a ``wtp_t`` instance. Workers obtain work from the
queue and, once a batch is available, release the queue lock and invoke
``pConsumer``【F:runtime/queue.c†L2179-L2222】.

Worker thread instance (wti)
----------------------------

Each running worker is represented by ``wti_t``. The instance keeps
thread identity and per-thread execution state, but also an array of
``actWrkrInfo`` records – one for every action in the configuration – so
that modules can store per-thread action data. The relevant fields are
shown in ``runtime/wti.h``【F:runtime/wti.h†L71-L90】【F:runtime/wti.h†L114-L122】.

When a worker terminates, it frees any action specific data and informs
the action object【F:runtime/wti.c†L418-L438】.

Action object
-------------

An ``action_t`` combines configuration and runtime state for a single
output action. Key members include:

- runtime control such as ``bDisabled`` and ``tLastExec``;
- ``pQueue`` referencing the action's queue;
- ``mutAction`` serializing sections that require exclusive access;
- ``mutWrkrDataTable`` and ``wrkrDataTable`` tracking per-worker action
  instances for HUP processing.

These members are defined in ``action.h``【F:action.h†L60-L110】.

When a worker needs a per-action instance, ``actionCheckAndCreateWrkrInstance``
creates it and stores the pointer in both the worker and the action's
``wrkrDataTable`` under ``mutWrkrDataTable`` protection【F:action.c†L918-L955】.

Action modules are invoked from ``processMsgMain`` for each message in a
batch. For complex cases, ``doSubmitToActionQComplex`` wraps the call to
``actionWriteToAction`` with ``mutAction`` to serialize access
【F:action.c†L1917-L1937】.

The simpler ``doSubmitToActionQ`` path does not lock ``mutAction`` before
updating fields like ``tLastExec``. If multiple worker threads are
configured for an action queue this may lead to concurrent updates; the
exact guarantees require further analysis.

wrkrInstanceData lifecycle
-------------------------

Each element in ``wti_t``'s ``actWrkrInfo`` array carries an
``actWrkrData`` pointer reserved for the module's per-thread state. The
pointer is created on first use by
``actionCheckAndCreateWrkrInstance``【F:action.c†L918-L955】 and stored both
in the worker and the action's ``wrkrDataTable`` so the action can later
free or inspect it. When the worker thread terminates, ``wti.c`` walks
over all ``actWrkrInfo`` entries, removes the worker from the action and
calls ``freeWrkrInstance`` for each non-null pointer
【F:runtime/wti.c†L418-L438】.

Because each worker owns its ``wrkrInstanceData`` exclusively, its fields
are usually accessed without extra synchronization. Modules only need
locks if they spawn helper threads that touch the same structure.

Function call flow
------------------

The main lifecycle of ``wrkrInstanceData`` is:

1. ``actionPrepare`` checks for a worker instance and invokes
   ``actionCheckAndCreateWrkrInstance`` to allocate one on demand
   【F:action.c†L918-L955】【F:action.c†L1011-L1036】.
2. Module entry points such as ``doAction`` and ``commitTransaction``
   receive the pointer stored in ``pWti->actWrkrInfo`` and operate on the
   worker-local state
   【F:action.c†L1214-L1233】【F:action.c†L1238-L1249】.
3. When a worker thread exits, ``wti.c`` iterates over all actions,
   removes the worker from each action's table, and calls
   ``freeWrkrInstance`` for every non-null pointer
   【F:runtime/wti.c†L414-L438】.

Module examples
---------------

* **omprog** – the per-worker structure holds the child process context
  used to execute the helper program; no locking is required because the
  worker thread is the sole user of this state
  【F:plugins/omprog/omprog.c†L110-L118】.
* **omhttp** – each worker keeps libcurl handles and buffers in
  ``wrkrInstanceData``【F:contrib/omhttp/omhttp.c†L174-L199】, which remain
  private to the worker.
* **mmkubernetes** – ``wrkrInstanceData`` stores per-thread libcurl
  handles and statistics counters
  【F:contrib/mmkubernetes/mmkubernetes.c†L193-L216】. The shared cache
  accessed by all workers resides in ``instanceData`` and is protected by
  ``cacheMtx`` outside the worker structure; the worker-local members are
  unsynchronized.

Shared instanceData and locking
-------------------------------

``instanceData`` is common to all workers of an action. Modules must
guard mutable members with their own primitives:

* **omfile** – ``mutWrite`` serializes writes to a file
  【F:tools/omfile.c†L153-L218】【F:tools/omfile.c†L1060-L1080】【F:tools/omfile.c†L1164-L1188】.
* **omelasticsearch** – ``mutErrFile`` protects the optional error log
  【F:plugins/omelasticsearch/omelasticsearch.c†L112-L118】【F:plugins/omelasticsearch/omelasticsearch.c†L1396-L1403】【F:plugins/omelasticsearch/omelasticsearch.c†L1468-L1470】.
* **ommysql** – a global read–write lock ``rwlock_hmysql`` wraps the
  per-worker MySQL handle even though each worker owns its connection
  【F:plugins/ommysql/ommysql.c†L110-L133】【F:plugins/ommysql/ommysql.c†L338-L347】.

pnLock in omazureeventhubs
--------------------------

``omazureeventhubs`` spawns a Proton event thread in addition to the
queue worker. Its ``wrkrInstanceData`` embeds the Proton handles and a
read–write lock ``pnLock``【F:plugins/omazureeventhubs/omazureeventhubs.c†L150-L187】.
``createWrkrInstance`` initializes the lock and starts the thread
【F:plugins/omazureeventhubs/omazureeventhubs.c†L540-L558】. The worker
thread acquires ``pnLock`` during connection setup and teardown
【F:plugins/omazureeventhubs/omazureeventhubs.c†L466-L487】【F:plugins/omazureeventhubs/omazureeventhubs.c†L588-L615】.

The Proton event loop manipulates ``pnProactor`` and ``pnConn`` without
taking this lock【F:plugins/omazureeventhubs/omazureeventhubs.c†L1096-L1115】,
so ``pnLock`` currently serializes only the worker's own setup and
cleanup steps. It is unclear whether this protection is effective or a
leftover from another module; further analysis is required to determine
if the lock can be removed or must also cover the Proton thread.

rwlock_hmysql in ommysql
-----------------------

``ommysql`` maintains a global read–write lock ``rwlock_hmysql`` that
guards the MySQL handle inside each worker instance. Read locks surround
queries and connection checks, while ``closeMySQL`` and ``initMySQL``
temporarily upgrade to a write lock before replacing the handle
【F:plugins/ommysql/ommysql.c†L154-L162】【F:plugins/ommysql/ommysql.c†L220-L272】.

The lock was introduced in commit
``a3b2983342e20d157b76695d162533c3cfa69587`` to avoid using stale or
NULL handles when reconnecting. No external references explaining the
bug were found. Because each worker owns its ``MYSQL`` pointer, the risk
of cross-thread reuse appears low; the commit's justification seems
speculative. We estimate a low probability (≈20%) that the race it guards
against can actually occur and recommend further review.

Concurrency considerations
--------------------------

- Multiple worker threads may be active for the same action when
  ``ActionQueueWorkerThreads`` is greater than one. The action object is
  shared among these threads, while ``actWrkrInfo`` and
  ``wrkrInstanceData`` provide thread-local storage.
- Modules must protect any mutable data in ``instanceData`` with their
  own locking primitives. The examples above show mutexes and
  read–write locks used for this purpose.
- ``mutAction`` covers only parts of the action's state. Fields updated
  outside that lock may see concurrent writes; developers should review
  the code before relying on serialization.

Open questions
--------------

* ``doSubmitToActionQ`` updates timing fields without holding
  ``mutAction``. It is unclear whether the core guarantees that only one
  worker thread executes this path at a time or whether data races are
  acceptable. Further investigation, potentially involving runtime
  tracing, is needed.
* Some older documentation claims that plugin entry points are never
  invoked concurrently for the same action. The current code allows
  multiple worker threads, so this guarantee may no longer hold.

