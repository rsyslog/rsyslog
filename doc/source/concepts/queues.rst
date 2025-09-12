Understanding rsyslog Queues
============================

Rsyslog uses queues whenever two activities need to be loosely coupled.
With a queue, one part of the system "produces" something while another
part "consumes" this something. The "something" is most often syslog
messages, but queues may also be used for other purposes.

This document provides a good insight into technical details, operation
modes and implications. In addition to it, an :doc:`rsyslog queue concepts
overview <../whitepapers/queues_analogy>` document exists which tries to explain
queues with the help of some analogies. This may probably be a better
place to start reading about queues. I assume that once you have
understood that document, the material here will be much easier to grasp
and look much more natural.

The most prominent example is the main message queue. Whenever rsyslog
receives a message (e.g. locally, via UDP, TCP or in whatever else way),
it places these messages into the main message queue. Later, it is
dequeued by the rule processor, which then evaluates which actions are
to be carried out. In front of each action, there is also a queue, which
potentially de-couples the filter processing from the actual action
(e.g. writing to file, database or forwarding to another host).

Where are Queues Used?
----------------------

Currently, queues are used for the main message queue and for the
actions.

There is a single main message queue inside rsyslog. Each input module
delivers messages to it. The main message queue worker filters messages
based on rules specified in rsyslog.conf and dispatches them to the
individual action queues. Once a message is in an action queue, it is
deleted from the main message queue.

There are multiple action queues, one for each configured action. By
default, these queues operate in direct (non-queueing) mode. Action
queues are fully configurable and thus can be changed to whatever is
best for the given use case.

Future versions of rsyslog will most probably utilize queues at other
places, too.

Wherever "*<object>*\ "  is used in the config file statements,
substitute "*<object>*\ " with either "MainMsg" or "Action". The former
will set main message queue parameters, the latter parameters for the
next action that will be created. Action queue parameters cannot be
modified once the action has been specified. For example, to tell the
main message queue to save its content on shutdown, use
*$MainMsgQueueSaveOnShutdown on*".

If the same parameter is specified multiple times before a queue is
created, the last one specified takes precedence. The main message queue
is created after parsing the config file and all of its potential
includes. An action queue is created each time an action selector is
specified. Action queue parameters are reset to default after an action
queue has been created (to provide a clean environment for the next
action).

Not all queues necessarily support the full set of queue configuration
parameters, because not all are applicable. For example, disk queues
always have exactly one worker thread. This cannot be overridden by
configuration parameters. Tries to do so are ignored.

Queue Modes
-----------

Rsyslog supports different queue modes, some with submodes. Each of them
has specific advantages and disadvantages. Selecting the right queue
mode is quite important when tuning rsyslogd. The queue mode (aka
"type") is set via the "*$<object>QueueType*\ " config directive.

Direct Queues
~~~~~~~~~~~~~

Direct queues are **non**-queuing queues. A queue in direct mode does
neither queue nor buffer any of the queue elements but rather passes the
element directly (and immediately) from the producer to the consumer.
This sounds strange, but there is a good reason for this queue type.

Direct mode queues allow to use queues generically, even in places where
queuing is not always desired. A good example is the queue in front of
output actions. While it makes perfect sense to buffer forwarding
actions or database writes, it makes only limited sense to build up a
queue in front of simple local file writes. Yet, rsyslog still has a
queue in front of every action. So for file writes, the queue mode can
simply be set to "direct", in which case no queuing happens.

Please note that a direct queue also is the only queue type that passes
back the execution return code (success/failure) from the consumer to
the producer. This, for example, is needed for the backup action logic.
Consequently, backup actions require the to-be-checked action to use a
"direct" mode queue.

To create a direct queue, use the "*$<object>QueueType Direct*\ " config
directive.

Disk Queues
~~~~~~~~~~~

Disk queues use disk drives for buffering. The important fact is that
they always use the disk and do not buffer anything in memory. Thus, the
queue is ultra-reliable, but by far the slowest mode. For regular use
cases, this queue mode is not recommended. It is useful if log data is
so important that it must not be lost, even in extreme cases.

When a disk queue is written, it is done in chunks. Each chunk receives
its individual file. Files are named with a prefix (set via the
"*$<object>QueueFilename*\ " config directive) and followed by a 7-digit
number (starting at one and incremented for each file). Chunks are 10mb
by default, a different size can be set via
the"*$<object>QueueMaxFileSize*\ " config directive. Note that the size
limit is not a sharp one: rsyslog always writes one complete queue
entry, even if it violates the size limit. So chunks are actually a
little bit (usually less than 1k) larger then the configured size. Each
chunk also has a different size for the same reason. If you observe
different chunk sizes, you can relax: this is not a problem.

Writing in chunks is used so that processed data can quickly be deleted
and is free for other uses - while at the same time keeping no
artificial upper limit on disk space used. If a disk quota is set
(instructions further below), be sure that the quota/chunk size allows
at least two chunks to be written. Rsyslog currently does not check that
and will fail miserably if a single chunk is over the quota.

Creating new chunks costs performance but provides quicker ability to
free disk space. The 10mb default is considered a good compromise
between these two. However, it may make sense to adapt these settings to
local policies. For example, if a disk queue is written on a dedicated
200gb disk, it may make sense to use a 2gb (or even larger) chunk size.

Please note, however, that the disk queue by default does not update its
housekeeping structures every time it writes to disk. This is for
performance reasons. In the event of failure, data will still be lost
(except when manually is mangled with the file structures). However,
disk queues can be set to write bookkeeping information on checkpoints
(every n records), so that this can be made ultra-reliable, too. If the
checkpoint interval is set to one, no data can be lost, but the queue is
exceptionally slow.

Each queue can be placed on a different disk for best performance and/or
isolation. This is currently selected by specifying different
*$WorkDirectory* config directives before the queue creation statement.

To create a disk queue, use the "*$<object>QueueType Disk*\ " config
directive. Checkpoint intervals can be specified via
"*$<object>QueueCheckpointInterval*\ ", with 0 meaning no checkpoints.
Note that disk-based queues can be made very reliable by issuing a
(f)sync after each write operation. Starting with version 4.3.2, this
can be requested via "*<object>QueueSyncQueueFiles on/off* with the
default being off. Activating this option has a performance penalty, so
it should not be turned on without reason.

If you happen to lose or otherwise need the housekeeping structures and 
have all yours queue chunks you can use perl script included in rsyslog
package to generate it. 
Usage: recover_qi.pl -w *$WorkDirectory* -f QueueFileName -d 8 > QueueFileName.qi


In-Memory Queues
~~~~~~~~~~~~~~~~

In-memory queue mode is what most people have on their mind when they
think about computing queues. Here, the enqueued data elements are held
in memory. Consequently, in-memory queues are very fast. But of course,
they do not survive any program or operating system abort (what usually
is tolerable and unlikely). Be sure to use a UPS if you use in-memory
mode and your log data is important to you. Note that even in-memory
queues may hold data for an infinite amount of time when e.g. an output
destination system is down and there is no reason to move the data out
of memory (lying around in memory for an extended period of time is NOT
a reason). Pure in-memory queues can't even store queue elements
anywhere else than in core memory.

There exist two different in-memory queue modes: LinkedList and
FixedArray. Both are quite similar from the user's point of view, but
utilize different algorithms.

A FixedArray queue uses a fixed, pre-allocated array that holds pointers
to queue elements. The majority of space is taken up by the actual user
data elements, to which the pointers in the array point. The pointer
array itself is comparatively small. However, it has a certain memory
footprint even if the queue is empty. As there is no need to dynamically
allocate any housekeeping structures, FixedArray offers the best run
time performance (uses the least CPU cycle). FixedArray is best if there
is a relatively low number of queue elements expected and performance is
desired. It is the default mode for the main message queue (with a limit
of 10,000 elements).

A LinkedList queue is quite the opposite. All housekeeping structures
are dynamically allocated (in a linked list, as its name implies). This
requires somewhat more runtime processing overhead, but ensures that
memory is only allocated in cases where it is needed. LinkedList queues
are especially well-suited for queues where only occasionally a
than-high number of elements need to be queued. A use case may be
occasional message burst. Memory permitting, it could be limited to e.g.
200,000 elements which would take up only memory if in use. A FixedArray
queue may have a too large static memory footprint in such cases.

**In general, it is advised to use LinkedList mode if in doubt**. The
processing overhead compared to FixedArray is low and may be outweighed by
the reduction in memory use. Paging in most-often-unused pointer array
pages can be much slower than dynamically allocating them.

To create an in-memory queue, use the "*$<object>QueueType
LinkedList*\ " or  "*$<object>QueueType FixedArray*\ " config directive.

Disk-Assisted Memory Queues
^^^^^^^^^^^^^^^^^^^^^^^^^^^

If a disk queue name is defined for in-memory queues (via
*$<object>QueueFileName*), they automatically become "disk-assisted"
(DA). In that mode, data is written to disk (and read back) on an
as-needed basis.

Actually, the regular memory queue (called the "primary queue") and a
disk queue (called the "DA queue") work in tandem in this mode. Most
importantly, the disk queue is activated if the primary queue is full or
needs to be persisted on shutdown. Disk-assisted queues combine the
advantages of pure memory queues with those of  pure disk queues. Under
normal operations, they are very fast and messages will never touch the
disk. But if there is need to, an unlimited amount of messages can be
buffered (actually limited by free disk space only) and data can be
persisted between rsyslogd runs.

With a DA-queue, both disk-specific and in-memory specific configuration
parameters can be set. From the user's point of view, think of a DA
queue like a "super-queue" which does all within a single queue [from
the code perspective, there is some specific handling for this case, so
it is actually much like a single object].

DA queues are typically used to de-couple potentially long-running and
unreliable actions (to make them reliable). For example, it is
recommended to use a disk-assisted linked list in-memory queue in front
of each database and "send via tcp" action. Doing so makes these actions
reliable and de-couples their potential low execution speed from the
rest of your rules (e.g. the local file writes). There is a howto on
`massive database inserts <rsyslog_high_database_rate.html>`_ which
nicely describes this use case. It may even be a good read if you do not
intend to use databases.

With DA queues, we do not simply write out everything to disk and then
run as a disk queue once the in-memory queue is full. A much smarter
algorithm is used, which involves a "high watermark" and a "low
watermark". Both specify numbers of queued items. If the queue size
reaches high watermark elements, the queue begins to write data elements
to disk. It does so until it reaches the low watermark elements. At
this point, it stops writing until either high watermark is reached
again or the on-disk queue becomes empty, in which case the queue
reverts back to in-memory mode, only. While holding at the low
watermark, new elements are actually enqueued in memory. They are
eventually written to disk, but only if the high watermark is ever
reached again. If it isn't, these items never touch the disk. So even
when a queue runs disk-assisted, there is in-memory data present (this
is a big difference to pure disk queues!).

This algorithm prevents unnecessary disk writes, but also leaves some
additional buffer space for message bursts. Remember that creating disk
files and writing to them is a lengthy operation. It is too lengthy to
e.g. block receiving UDP messages. Doing so would result in message
loss. Thus, the queue initiates DA mode, but still is able to receive
messages and enqueue them - as long as the maximum queue size is not
reached. The number of elements between the high watermark and the
maximum queue size serves as this "emergency buffer". Size it according
to your needs, if traffic is very bursty you will probably need a large
buffer here. Keep in mind, though, that under normal operations these
queue elements will probably never be used. Setting the high watermark
too low will cause disk-assistance to be turned on more often than
actually needed.

The watermarks can be set via the "*$<object>QueueHighWatermark*\ "
and  "*$<object>QueueLowWatermark*\ " configuration file directives.
Note that these are actual numbers, not percentages. Be sure they make
sense (also in respect to "*$<object>QueueSize*\ "). Rsyslogd does
perform some checks on the numbers provided, and issues warning when
numbers are "suspicious".

Limiting the Queue Size
-----------------------

All queues, including disk queues, have a limit of the number of
elements they can enqueue. This is set via the "*$<object>QueueSize*\ "
config parameter. Note that the size is specified in number of enqueued
elements, not their actual memory size. Memory size limits can not be
set. A conservative assumption is that a single syslog messages takes up
512 bytes on average (in-memory, NOT on the wire, this \*is\* a
difference).

Disk assisted queues are special in that they do **not** have any size
limit. They enqueue an unlimited amount of elements. To prevent running
out of space, disk and disk-assisted queues can be size-limited via the
"*$<object>QueueMaxDiskSpace*\ " configuration parameter. If it is not
set, the limit is only available free space (and reaching this limit is
currently not very gracefully handled, so avoid running into it!). If a
limit is set, the queue can not grow larger than it. Note, however, that
the limit is approximate. The engine always writes complete records. As
such, it is possible that slightly more than the set limit is used
(usually less than 1k, given the average message size). Keeping strictly
on the limit would be a performance hurt, and thus the design decision
was to favour performance. If you don't like that policy, simply specify
a slightly lower limit (e.g. 999,999K instead of 1G).

In general, it is a good idea to limit the physical disk space even if
you dedicate a whole disk to rsyslog. That way, you prevent it from
running out of space (future version will have an auto-size-limit logic,
that then kicks in in such situations).

Worker Thread Pools
-------------------

Each queue (except in "direct" mode) has an associated pool of worker
threads. Worker threads carry out the action to be performed on the data
elements enqueued. As an actual sample, the main message queue's worker
task is to apply filter logic to each incoming message and enqueue them
to the relevant output queues (actions).

Worker threads are started and stopped on an as-needed basis. On a
system without activity, there may be no worker at all running. One is
automatically started when a message comes in. Similarly, additional
workers are started if the queue grows above a specific size. The
"*$<object>QueueWorkerThreadMinimumMessages*\ "  config parameter
controls worker startup. If it is set to the minimum number of elements
that must be enqueued in order to justify a new worker startup. For
example, let's assume it is set to 100. As long as no more than 100
messages are in the queue, a single worker will be used. When more than
100 messages arrive, a new worker thread is automatically started.
Similarly, a third worker will be started when there are at least 300
messages, a fourth when reaching 400 and so on.

It, however, does not make sense to have too many worker threads running
in parallel. Thus, the upper limit can be set via
"*$<object>QueueWorkerThreads*\ ". If it, for example, is set to four,
no more than four workers will ever be started, no matter how many
elements are enqueued.

Worker threads that have been started are kept running until an
inactivity timeout happens. The timeout can be set via
"*$<object>QueueWorkerTimeoutThreadShutdown*\ " and is specified in
milliseconds. If you do not like to keep the workers running, simply set
it to 0, which means immediate timeout and thus immediate shutdown. But
consider that creating threads involves some overhead, and this is why
we keep them running. If you would like to never shutdown any worker
threads, specify -1 for this parameter.

Discarding Messages
~~~~~~~~~~~~~~~~~~~

If the queue reaches the so called "discard watermark" (a number of
queued elements), less important messages can automatically be
discarded. This is in an effort to save queue space for more important
messages, which you even less like to lose. Please note that whenever
there are more than "discard watermark" messages, both newly incoming as
well as already enqueued low-priority messages are discarded. The
algorithm discards messages newly coming in and those at the front of
the queue.

The discard watermark is a last resort setting. It should be set
sufficiently high, but low enough to allow for large message burst.
Please note that it takes effect immediately and thus shows effect
promptly - but that doesn't help if the burst mainly consist of
high-priority messages...

The discard watermark is set via the "*$<object>QueueDiscardMark*\ "
directive. The priority of messages to be discarded is set via
"*$<object>QueueDiscardSeverity*\ ". This directive accepts both the
usual textual severity as well as a numerical one. To understand it, you
must be aware of the numerical severity values. They are defined in RFC
3164:

        ==== ========
        Code Severity
        ==== ========
        0    Emergency: system is unusable
        1    Alert: action must be taken immediately
        2    Critical: critical conditions
        3    Error: error conditions
        4    Warning: warning conditions
        5    Notice: normal but significant condition
        6    Informational: informational messages
        7    Debug: debug-level messages
        ==== ========

Anything of the specified severity and (numerically) above it is
discarded. To turn message discarding off, simply specify the discard
watermark to be higher than the queue size. An alternative is to specify
the numerical value 8 as DiscardSeverity. This is also the default
setting to prevent unintentional message loss. So if you would like to
use message discarding, you need to set"
*$<object>QueueDiscardSeverity*" to an actual value.

An interesting application is with disk-assisted queues: if the discard
watermark is set lower than the high watermark, message discarding will
start before the queue becomes disk-assisted. This may be a good thing
if you would like to switch to disk-assisted mode only in cases where it
is absolutely unavoidable and you prefer to discard less important
messages first.

Filled-Up Queues
----------------

If the queue has either reached its configured maximum number of entries
or disk space, it is finally full. If so, rsyslogd throttles the data
element submitter. If that, for example, is a reliable input (TCP, local
log socket), that will slow down the message originator which is a good
resolution for this scenario.

During throttling, a disk-assisted queue continues to write to disk and
messages are also discarded based on severity as well as regular
dequeuing and processing continues. So chances are good the situation
will be resolved by simply throttling. Note, though, that throttling is
highly undesirable for unreliable sources, like UDP message reception.
So it is not a good thing to run into throttling mode at all.

We can not hold processing infinitely, not even when throttling. For
example, throttling the local log socket too long would cause the
system at whole come to a standstill. To prevent this, rsyslogd times
out after a configured period ("*$<object>QueueTimeoutEnqueue*\ ",
specified in milliseconds) if no space becomes available. As a last
resort, it then discards the newly arrived message.

If you do not like throttling, set the timeout to 0 - the message will
then immediately be discarded. If you use a high timeout, be sure you
know what you do. If a high main message queue enqueue timeout is set,
it can lead to something like a complete hang of the system. The same
problem does not apply to action queues.

Rate Limiting
~~~~~~~~~~~~~

Rate limiting provides a way to prevent rsyslogd from processing things
too fast. It can, for example, prevent overrunning a receiver system.

Currently, there are only limited rate-limiting features available. The
"*$<object>QueueDequeueSlowdown*\ "  directive allows to specify how
long (in microseconds) dequeueing should be delayed. While simple, it
still is powerful. For example, using a DequeueSlowdown delay of 1,000
microseconds on a UDP send action ensures that no more than 1,000
messages can be sent within a second (actually less, as there is also
some time needed for the processing itself).

Processing Timeframes
~~~~~~~~~~~~~~~~~~~~~

Queues can be set to dequeue (process) messages only during certain
timeframes. This is useful if you, for example, would like to transfer
the bulk of messages only during off-peak hours, e.g. when you have only
limited bandwidth on the network path to the central server.

Currently, only a single timeframe is supported and, even worse, it can
only be specified by the hour. It is not hard to extend rsyslog's
capabilities in this regard - it was just not requested so far. So if
you need more fine-grained control, let us know and we'll probably
implement it. There are two configuration directives, both should be
used together or results are unpredictable:"
*$<object>QueueDequeueTimeBegin <hour>*"
and "*$<object>QueueDequeueTimeEnd <hour>*\ ". The hour parameter must
be specified in 24-hour format (so 10pm is 22). A use case for this
parameter can be found in the `rsyslog
wiki <http://wiki.rsyslog.com/index.php/OffPeakHours>`_.

Performance
~~~~~~~~~~~

The locking involved with maintaining the queue has a potentially large
performance impact. How large this is, and if it exists at all, depends
much on the configuration and actual use case. However, the queue is
able to work on so-called "batches" when dequeueing data elements. With
batches, multiple data elements are dequeued at once (with a single
locking call). The queue dequeues all available elements up to a
configured upper limit (*<object>DequeueBatchSize <number>*). It is
important to note that the actual upper limit is dictated by
availability. The queue engine will never wait for a batch to fill. So
even if a high upper limit is configured, batches may consist of fewer
elements, even just one, if there are no more elements waiting in the
queue.

Batching can improve performance considerably. Note, however, that it
affects the order in which messages are passed to the queue worker
threads, as each worker now receive as batch of messages. Also, the
larger the batch size and the higher the maximum number of permitted
worker threads, the more main memory is needed. For a busy server, large
batch sizes (around 1,000 or even more elements) may be useful. Please
note that with batching, the main memory must hold BatchSize \*
NumOfWorkers objects in memory (worst-case scenario), even if running in
disk-only mode. So if you use the default 5 workers at the main message
queue and set the batch size to 1,000, you need to be prepared that the
main message queue holds up to 5,000 messages in main memory **in
addition** to the configured queue size limits!

The queue object's default maximum batch size is eight, but there exists
different defaults for the actual parts of rsyslog processing that
utilize queues. So you need to check these object's defaults.

Terminating Queues
~~~~~~~~~~~~~~~~~~

Terminating a process sounds easy, but can be complex. Terminating a
running queue is in fact the most complex operation a queue object can
perform. You don't see that from a user's point of view, but it's quite
hard work for the developer to do everything in the right order.

The complexity arises when the queue has still data enqueued when it
finishes. Rsyslog tries to preserve as much of it as possible. As a
first measure, there is a regular queue time out
("*$<object>QueueTimeoutShutdown*\ ", specified in milliseconds): the
queue workers are given that time period to finish processing the queue.

If after that period there is still data in the queue, workers are
instructed to finish the current data element and then terminate. This
essentially means any other data is lost. There is another timeout
("*$<object>QueueTimeoutActionCompletion*\ ", also specified in
milliseconds) that specifies how long the workers have to finish the
current element. If that timeout expires, any remaining workers are
cancelled and the queue is brought down.

If you do not like to lose data on shutdown, the
"*$<object>QueueSaveOnShutdown*\ " parameter can be set to "on". This
requires either a disk or disk-assisted queue. If set, rsyslogd ensures
that any queue elements are saved to disk before it terminates. This
includes data elements there were begun being processed by workers that
needed to be cancelled due to too-long processing. For a large queue,
this operation may be lengthy. No timeout applies to a required shutdown
save.
