.. _queueworker_and_action_instances:

Queue Worker and Action Instances
=================================

This document describes the rsyslog queue and worker thread model, with a
focus on how actions and action instances are handled in a multi-threaded
environment. Understanding this model is crucial for developers writing
output modules, as it dictates where locking is required and where it is not.

High-Level Overview
-------------------

Rsyslog uses a powerful queueing system to buffer messages between different
processing stages. This allows for high performance and reliability, as it
decouples message ingestion from output processing. Each queue can have a
pool of worker threads that process messages from the queue.

When a message is passed to an action, it is first enqueued in the action's
queue. A worker thread from the queue's worker thread pool then dequeues the
message and passes it to the action's output module for processing.

Action and Action Instance Objects
----------------------------------

The two key data structures for understanding the threading model are the
`action_s` (action) and `actWrkrInfo_t` (action instance) objects.

*   **`action_s` (action)**: This structure represents a configured output
    action (e.g., writing to a file, sending to a remote server). It contains
    the action's configuration, its queue, and other global data. There is
    only one `action_s` object per configured action.

*   **`actWrkrInfo_t` (action instance)**: This structure represents the state
    of an action for a specific worker thread. It contains data that is local
    to the worker thread, such as the action's state (e.g., ready, suspended),
    retry counters, and transaction parameters. Each worker thread has its
    own `actWrkrInfo_t` object for each action.

*   **`wrkrInstanceData_t` (worker instance data)**: This is a private data
    structure that can be defined by an output module to store data that is
    specific to a worker thread. It is created in the `createWrkrInstance`
    function and freed in `freeWrkrInstance`. The `wrkrInstanceData_t` is
    accessible from the action's entry points via the `pWti` (worker thread
    instance) pointer.

Threading Model
---------------

Rsyslog's threading model is designed to be both efficient and easy to use for
plugin developers. The key principle is that **an action's entry points are
never called concurrently for the same action instance**. This means that a
plugin developer does not need to worry about locking within the action's
entry points, as long as they only access data in the `actWrkrInfo_t` or
`wrkrInstanceData_t` objects.

However, it is important to remember that **different worker threads can
access the same `action_s` object concurrently**. Therefore, any access to
data in the `action_s` object must be protected by a mutex. The `action_s`
structure has a `mutAction` mutex for this purpose.

Here's a summary of the locking rules:

*   **`action_s`**: All access to data in the `action_s` object must be
    protected by the `mutAction` mutex.
*   **`actWrkrInfo_t`**: No locking is required for data in the `actWrkrInfo_t`
    object, as it is only ever accessed by a single worker thread.
*   **`wrkrInstanceData_t`**: No locking is required for data in the
    `wrkrInstanceData_t` object, as it is only ever accessed by a single
    worker thread.

The `omazureeventhubs` and `ommysql` output modules are notable exceptions to this rule.
They both use locks to protect data structures that are shared between threads.

Queue Types and Threading
-------------------------

The type of queue used by an action can have a significant impact on the
threading model.

*   **Direct Queues**: Direct queues do not have their own worker threads.
    Instead, the worker thread from the main message queue calls the action's
    entry points directly. This is the most efficient queue type, but it
    should only be used for actions that are guaranteed not to block.

*   **In-Memory, Disk, and Disk-Assisted Queues**: These queue types all have
    their own worker thread pools. When a message is enqueued, a worker
    thread from the pool dequeues it and calls the action's entry points.
    This decouples the main message queue from the output action, which is
    essential for actions that may block (e.g., writing to a remote server).

Examples
--------

Let's look at how some of the built-in output modules use the queueing and
threading model.

*   **`omfile`**: This is a simple, non-transactional output module that
    writes messages to a file. It uses a direct queue by default, as writing
    to a local file is generally a non-blocking operation. It does not use
    `wrkrInstanceData`.

*   **`omfwd`**: This module uses `wrkrInstanceData` to store the state of the
    connection to the remote server, including the socket, the send buffer,
    and the retry timer. This data is specific to each worker thread, so no
    locking is required.

*   **`omelasticsearch`**: This is a transactional output module that sends
    messages to Elasticsearch. It uses `wrkrInstanceData` to store the curl
    handle, the response buffer, and the batch of messages to be sent. This
    data is specific to each worker thread, so no locking is required.

*   **`omazureeventhubs`**: This is another transactional output module that
    sends messages to Azure Event Hubs. It uses `wrkrInstanceData` to store
    the Proton-related data structures. It also uses a `pthread_rwlock_t` to
    protect these data structures from concurrent access by the rsyslog worker
    thread and the Proton handler thread.

*   **`ommysql`**: This module uses `wrkrInstanceData` to store the MySQL
    connection handle. It uses a `pthread_rwlock_t` to protect the connection
    handle from concurrent access.

The `pnLock` in `omazureeventhubs`
----------------------------------

The `omazureeventhubs` module uses a separate thread to handle the AMQP
protocol via the qpid-proton library. This thread is created in
`createWrkrInstance` and runs the `proton_thread` function. The main rsyslog
worker thread communicates with the proton thread via the data structures
in `wrkrInstanceData_t`.

The `pnLock` is a `pthread_rwlock_t` that is used to protect these data
structures from concurrent access. The main worker thread acquires a write
lock when it needs to modify the data structures (e.g., in `setupProtonHandle`
and `closeProton`), and the proton thread is expected to acquire a read lock
when it needs to read them. This prevents race conditions and ensures that the
data structures are always in a consistent state.

While the lock is necessary for thread safety, it is worth noting that the
proton thread never actually acquires a read lock. This is a potential bug,
but it may not cause problems in practice if the main worker thread is the
only thread that modifies the data structures. However, it would be better
to acquire a read lock in the proton thread to be safe.

The `rwlock_hmysql` in `ommysql`
--------------------------------

The `ommysql` module uses a `pthread_rwlock_t` to protect the `hmysql`
connection handle. The lock is acquired for reading in `tryResume` and
`commitTransaction`, and for writing in `createWrkrInstance`,
`freeWrkrInstance`, `closeMySQL`, and `initMySQL`.

The commit that introduced this lock suggests that it was added to prevent
a race condition where one thread could be using a stale or NULL connection
handle while another thread is re-establishing the connection. However,
since `wrkrInstanceData` is supposed to be thread-local, it is unclear
if this lock is strictly necessary. It is possible that it was added as a
precaution, or that there is a subtle issue with the MySQL client library's
thread safety that is not immediately apparent.

Control Flow for `wrkrInstanceData`
-------------------------------------

The `wrkrInstanceData_t` struct is a key part of the rsyslog output module
API. It allows modules to store private, thread-local data that is
associated with a specific worker thread. Here is a summary of the control
flow for `wrkrInstanceData`:

1.  **`createWrkrInstance`**: This function is called by the rsyslog core when
    a new worker thread is created for an action. The output module should
    allocate and initialize its `wrkrInstanceData_t` struct in this function.

2.  **Action Entry Points**: The `wrkrInstanceData_t` struct is passed to the
    action's entry points (e.g., `doAction`, `commitTransaction`) via the
    `pWrkrData` parameter. The output module can then use this data to
    maintain its state for the current worker thread.

3.  **`freeWrkrInstance`**: This function is called by the rsyslog core when
    a worker thread is destroyed. The output module should free any resources
    that were allocated in `createWrkrInstance`.

Doubts and Further Refinements
------------------------------

The documentation of the queueing system in `doc/source/development/dev_queue.rst`
is outdated. It would be beneficial to update this document to reflect the
current state of the code.

The interaction between the different queue types and the threading model can
be complex. A more detailed diagram of the threading model would be helpful
for developers.

The error handling in the output modules can be inconsistent. A set of
best practices for error handling in output modules would be a valuable
addition to the documentation.
