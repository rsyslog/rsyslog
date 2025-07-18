************************
General Queue Parameters
************************

===========================  ===========================================================================
**Authors:**                  `Rainer Gerhards <https://rainer.gerhards.net/>`_ <rgerhards@adiscon.com>;
===========================  ===========================================================================


Usage
=====

Queue parameters can be used together with the following statements:

- :doc:`action() <../configuration/actions>`
- ruleset()
- main\_queue()

Queues need to be configured in the action or ruleset it should affect.
If nothing is configured, default values will be used. Thus, the default
ruleset has only the default main queue. Specific Action queues are not
set up by default.

To fully understand queue parameters and how they interact, be sure to
read the :doc:`queues <../concepts/queues>` documentation.


Configuration Parameters
========================

.. note::

   As with other configuration objects, parameters for this
   object are case-insensitive.


queue.filename
--------------

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "none", "no", "``$ActionQueueFileName``"

File name to be used for the queue files. If specified, this parameter
enables disk-assisted queue functionality. If *not* specified,
the queue will operate without saving the queue to disk, either
during its operation or when shut down. See the separate
``queue.saveonshutdown`` parameter to configure that option.
Please note that this is actually just the file name. A directory
can NOT be specified in this parameter. If the files shall be
created in a specific directory, specify ``queue.spoolDirectory``
for this. The filename is used to build to complete path for queue
files.


queue.spoolDirectory
--------------------

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "none", "no", "none"

This is the directory into which queue files will be stored. Note
that the directory must exist, it is NOT automatically created by
rsyslog. If no spoolDirectory is specified, the work directory is
used.


queue.size
----------

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "1000/50000", "no", "``$ActionQueueSize``"

This is the maximum size of the queue in number of messages. Note
that setting the queue size to very small values (roughly below 100
messages) is not supported and can lead to unpredictable results.
For more information on the current status of this restriction see
the `rsyslog FAQ: "lower bound for queue
sizes" <http://www.rsyslog.com/lower-bound-for-queue-sizes/>`_.

The default depends on queue type and rsyslog version, if you need
a specific value, please specify it. Otherwise rsyslog selects what
it considers appropriate for the version in question. In rsyslog
rsyslog 8.30.0, for example, ruleset queues have a default size
of 50000 and action queues which are configured to be non-direct
have a size of 1000.


queue.dequeueBatchSize
----------------------

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "128/1024", "no", "``$ActionQueueDequeueBatchSize``"

Specifies the maximum batch size for dequeue operations. This setting affects performance.
As a rule of thumb, larger batch sizes (up to a environment-induced upper limit)
provide better performance. For the average system, there usually should be no need
to adjust batch sizes as the defaults are sufficient. The default for ruleset queues
is 1024 and for action queues 128.

Note that this only specifies the **maximum** batch size. Batches will be slower if
rsyslog does not have as many messages inside the queue at time of dequeuing it.
If you want to set a minimum Batch size as well, you can use `queue.minDequeueBatchSize`.


queue.minDequeueBatchSize
-------------------------

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "0", "no", "none"

Specifies the **minimum** batch size for dequeue operations. This setting is especially
useful with outputs like ElasticSearch or ClickHouse, where you want to limit the
number of HTTP requests. With this setting, the queue engine waits up to
`queue.minDequeueBatchSize.timeout` milliseconds when there are fewer messages
currently queued. Note that the minimum batch size cannot be larger than the
configured maximum batch size. If so, it is automatically adjusted to
match the maximum. So if in doubt, you need to specify both parameters.


queue.minDequeueBatchSize.timeout
---------------------------------

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "1000", "no", "none"

This parameter is only meaningful if use together with `queue.minDequeueBatchSize`,
otherwise it is ignored. It specifies the amount of time (in milliseconds) rsyslogs
waits for new
messages so that the minimum batch size can be reached. After this period, the
batch is processed, *even if it is below minimum size*. This capability exists to
prevent having messages stalled in an incomplete batch just because no new
messages arrive. We would expect that it usually makes little sense to set it
to higher than 60.000 (60 seconds), but this is permitted. Just be warned that
this potentially delays log processing for that long.


queue.maxDiskSpace
------------------

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "0", "no", "``$ActionQueueMaxDiskSpace``"

The maximum size that all queue files together will use on disk. Note
that the actual size may be slightly larger than the configured max,
as rsyslog never writes partial queue records.


queue.highWatermark
-------------------

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "90% of queue.size", "no", "``$ActionQueueHighWaterMark``"

This applies to disk-assisted queues, only. When the queue fills up
to this number of messages, the queue begins to spool messages to
disk. Please note that this should not happen as part of usual
processing, because disk queue mode is very considerably slower than
in-memory queue mode. Going to disk should be reserved for cases
where an output action destination is offline for some period.


queue.lowWatermark
------------------

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "70% of queue.size", "no", "``$ActionQueueLowWaterMark``"

This applies to disk-assisted queues, only. When the high watermark is
reached, the queue will write data to disk. It does so until the low
watermark is reached, then the queue reverts back to in-memory mode.


queue.fullDelaymark
-------------------

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "97% of queue.size", "no", "none"

Number of messages when the queue should block delayable messages.
Messages are NO LONGER PROCESSED until the queue has sufficient space
again. If a message is delayable depends on the input. For example,
messages received via imtcp are delayable (because TCP can push back),
but those received via imudp are not (as UDP does not permit a push back).
The intent behind this setting is to leave some space in an almost-full
queue for non-delayable messages, which would be lost if the queue runs
out of space. Please note that if you use a DA queue, setting the
fulldelaymark BELOW the highwatermark makes the queue never activate
disk mode for delayable inputs. So this is probably not what you want.


queue.lightDelayMark
--------------------

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "70% of queue.size", "no", "none"

If this mark is reached the sender will be throttled if possible. The
main idea to do this is leave some space inside the queue for inputs
like UDP which cannot be throttled - and so any data arriving at
"queue full" would be discarded.

If the special value `0` is used, `queue.LightDelayMark` will be set
to the value of `queue.size`. This effectively **disables** light delay
functionality. This is useful if a queue is not used by non-delayable
inputs like UDP. The special value was introduced in rsyslog 8.1904.0
and is **not** available in earlier versions. There, you can achieve the
same result by setting `queue.LightDelayMark` to a very large value.


queue.discardMark
-----------------

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "98% of queue.size", "no", "``$ActionQueueDiscardMark``"

Specifies the threshold at which rsyslog begins to discard less important
messages. To define which messages should be discarded use the
queue.discardseverity parameter.


queue.discardSeverity
---------------------

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "8", "no", "``$ActionQueueDiscardSeverity``"

As soon as the threshold of the parameter queue.discardMark is reached
incoming as well as queued messages with a priority equal or lower than
specified will be erased. With the default no messages will be erased.
You have to specify a numeric severity value for this parameter.


queue.checkpointInterval
------------------------

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "0", "no", "``$ActionQueueCheckpointInterval``"

Disk queues by default do not update housekeeping structures every time
the queue writes to disk. This is for performance reasons. In the event of failure,
data will still be lost (except when data is mangled via the file structures).
However, disk queues can be set to write bookkeeping information on checkpoints
(every n records), so that this can be made ultra-reliable, too. If the
checkpoint interval is set to one, no data can be lost, but the queue is
exceptionally slow.


queue.syncqueuefiles
--------------------

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "binary", "off", "no", "``$ActionQueueSyncQueueFiles``"

Disk-based queues can be made very reliable by issuing a (f)sync after each
write operation. This happens when you set the parameter to "on".
Activating this option has a performance penalty, so it should not
be turned on without a good reason. Note that the penalty also depends on
*queue.checkpointInterval* frequency.


queue.samplingInterval
----------------------

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "0", "no", "none"

.. versionadded:: 8.23.0

This option allows queues to be populated by events produced at a specific interval.
It provides a way to sample data each N events, instead of processing all, in order
to reduce resources usage (disk, bandwidth...)


queue.type
----------

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "Direct", "no", "``$ActionQueueType``"

Specifies the type of queue that will be used. Possible options are "FixedArray",
"LinkedList", "Direct" or "Disk". For more information read the documentation
for :doc:`queues <../concepts/queues>`.


queue.workerThreads
-------------------

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "1", "no", "``$ActionQueueWorkerThreads``"

Specifies the maximum number of worker threads that can be run parallel.


queue.workerThreadMinimumMessages
---------------------------------

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "queue.size/queue.workerthreads", "no", "``$ActionQueueWorkerThreadMinimumMessages``"

Specify the number of messages a worker thread is processing before another
worker thread is created. This number is limited by parameter queue.workerThreads.
For example if this parameter is set to 200 and in the queue are 201 messages a
second worker thread will be created.


queue.timeoutWorkerthreadShutdown
---------------------------------

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "60000", "no", "``$ActionQueueTimeoutWorkerthreadShutdown``"

After starting a worker thread, it will process messages until there are no
messages for him to process. This parameter specifies the time the worker
thread has to be inactive before it times out.
The parameter must be specified in milliseconds. Which means the default of
60000 is 1 minute.


queue.timeoutshutdown
---------------------

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "10/1500", "no", "``$ActionQueueTimeoutShutdown``"

If a queue that still contains messages is terminated it will wait the
specified time interval for the worker thread to finish.
The time is specified in milliseconds (1000ms is 1sec).
Default for action queues is 10, for ruleset queues it is 1500.


queue.timeoutActionCompletion
-----------------------------

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "1000", "no", "``$ActionQueueTimeoutActionCompletion``"

When a queue is terminated, the timeout shutdown is over and there is
still data in the queue, the queue will finish the current data element
and then terminate. This parameter specifies the timeout for processing
this last element.
Parameter is specified in milliseconds (1000ms is 1sec).


queue.timeoutEnqueue
--------------------

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "2000", "no", "``$ActionQueueTimeoutEnqueue``"

This timeout value is used when the queue is full. If rsyslog cannot
enqueue a message within the timeout period, the message is discarded.
Note that this is setting of last resort (assuming defaults are used
for the queue settings or proper parameters are set): all delayable
inputs (like imtcp or imfile) have already been pushed back at this
stage. Also, discarding of lower priority messages (if configured) has
already happened. So we run into one of these situations if we do not
timeout quickly enough:

* if using imuxsock and no systemd journal is involved, the system
  would become unresponsive and most probably a hard reset would be
  required.
* if using imuxsock with imjournal forwarding is active, messages are
  lost because the journal discards them (more aggressive than rsyslog does)
* if using imjournal, the journal will buffer messages. If journal
  runs out of configured space, messages will be discarded. So in this
  mode discarding is moved to a bit later place.
* other non-delayable sources like imudp will also loose messages

So this setting is provided in order to guard against problematic situations,
which always will result either in message loss or system hang. For
action queues, one may debate if it would be better to overflow rapidly
to the main queue. If so desired, this is easy to accomplish by setting
a very large timeout value. The same, of course, is true for the main
queue, but you have been warned if you do so!

In some other words, you can consider this scenario, using default values.
With all progress blocked (unable to deliver a message):

* all delayable inputs (tcp, relp, imfile, imjournal, etc) will block
  indefinitely (assuming queue.lightdelaymark and queue.fulldelaymark
  are set sensible, which they are by default).
* imudp will be loosing messages because the OS will be dropping them
* messages arriving via UDP or imuxsock that do make it to rsyslog,
  and that are a severity high enough to not be filtered by
  discardseverity, will block for 2 seconds trying to put the message in
  the queue (in the hope that something happens to make space in the
  queue) and then be dropped to avoid blocking the machine permanently.

  Then the next message to be processed will also be tried for 2 seconds, etc.

* If this is going into an action queue, the log message will remain
  in the main queue during these 2 seconds, and additional logs that
  arrive will accumulate behind this in the main queue.


queue.maxFileSize
-----------------

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "1m/16m", "no", "``$ActionQueueMaxFileSize``"

Specifies the maximum size for the disk-assisted queue file.
Parameter can be specified in Mebibyte or Gibibyte, default for action
queues is 1m and for ruleset queues 16m (1m = 1024*1024).


queue.saveOnShutdown
--------------------

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "binary", "off", "no", "``$ActionQueueSaveOnShutdown``"

This parameter specifies if data should be saved at shutdown.


queue.dequeueSlowDown
---------------------

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "0", "no", "``$ActionQueueDequeueSlowDown``"

Regulates how long dequeueing should be delayed. This value must be specified
in microseconds (1000000us is 1sec). It can be used to slow down rsyslog so
it won't send things to fast.
For example if this parameter is set to 10000 on a UDP send action, the action
won't be able to put out more than 100 messages per second.


queue.dequeueTimeBegin
----------------------

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "0", "no", "``$ActionQueueDequeueTimeBegin``"

With this parameter you can specify rsyslog to process queues during specific
time periods. To define a time frame use the 24-hour format without minutes.
This parameter specifies the begin and "queue.dequeuetimeend" the end of the
time frame.


queue.dequeueTimeEnd
--------------------

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "25", "no", "``$ActionQueueDequeueTimeEnd``"

With this parameter you can specify rsyslog to process queues during specific
time periods. To define a time frame use the 24-hour format without minutes.
This parameter specifies the end and "queue.dequeuetimebegin" the begin of the
time frame. The default 25 disables the time-window.


queue.takeFlowCtlFromMsg
------------------------

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "boolean", "off", "no", "none"

.. versionadded:: 8.1911.0

This is a fine-tuning parameter which permits to control whether or not
rsyslog shall always take the flow control setting from the message. If
so, non-primary queues may also **block** when reaching high water mark.

This permits to add some synchronous processing to rsyslog core engine.
However, **this involves some risk**:  Improper use may make the core engine
stall. As such, **enabling this parameter requires very careful planning
of the rsyslog configuration and deep understanding of the consequences**.

Note that the parameter is applied to individual queues, so a configuration
with a large number of queues can (and must if used) be fine-tuned to
the exact use case.

**The rsyslog team strongly recommends to let this parameter turned off.**



Examples
========

Example 1
---------

The following is a sample of a TCP forwarding action with its own queue.

.. code-block:: none

   action(type="omfwd" target="192.168.2.11" port="10514" protocol="tcp"
          queue.filename="forwarding" queue.size="1000000" queue.type="LinkedList"
         )

