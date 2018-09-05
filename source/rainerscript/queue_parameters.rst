General Queue Parameters
------------------------

Queue parameters can be used together with the following statements:

- :doc:`action() <../configuration/actions>`
- ruleset()
- main\_queue()

Queues need to be configured in the action or ruleset it should affect.
If nothing is configured, default values will be used. Thus, the default
ruleset has only the default main queue. Specific Action queues are not
set up by default.

To fully understand queue parameters and how they interact, be sure to
read the :doc:`queues <../concepts/queues>` documentation. Note:
As with other configuration objects, parameters for this
object are case-insensitive.

-  **queue.filename** name
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
-  **queue.spoolDirectory** name
   This is the directory into which queue files will be stored. Note
   that the directory must exist, it is NOT automatically created by
   rsyslog. If no spoolDirectory is specified, the work directory is
   used.
-  **queue.size** number
   This is the maximum size of the queue in number of messages. Note
   that setting the queue size to very small values (roughly below 100
   messages) is not supported and can lead to unpredictable results.
   For more information on the current status of this restriction see
   the `rsyslog FAQ: "lower bound for queue
   sizes" <http://www.rsyslog.com/lower-bound-for-queue-sizes/>`_.

   The default depends on queue type and rsyslog version, if you need
   a specific value, please specify it. Otherwise rsyslog selects what
   it consideres appropriate for the version in question. In rsyslog
   rsyslog 8.30.0, for example, ruleset queues have a default size
   of 50000 and action queues which are configured to be non-direct
   have a size of 1000.
-  **queue.dequeuebatchsize** number
   default 128
-  **queue.maxdiskspace** number
   The maximum size that all queue files together will use on disk. Note
   that the actual size may be slightly larger than the configured max,
   as rsyslog never writes partial queue records.
-  **queue.highwatermark** number
   This applies to disk-assisted queues, only. When the queue fills up
   to this number of messages, the queue begins to spool messages to
   disk. Please note that this should not happen as part of usual
   processing, because disk queue mode is very considerably slower than
   in-memory queue mode. Going to disk should be reserved for cases
   where an output action destination is offline for some period.
   default 90% of queue size
-  **queue.lowwatermark** number
   default 70% of queue size
-  **queue.fulldelaymark** number
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
   default 97% of queue size
-  **queue.lightdelaymark** number
   default 70% of queue size
-  **queue.discardmark** number
   default 80% of queue size
-  **queue.discardseverity** number
   \*numerical\* severity! default 8 (nothing discarded)
-  **queue.checkpointinterval** number
   Disk queues by default do not update housekeeping structures every time
   the queue writes to disk. This is for performance reasons. In the event of failure,
   data will still be lost (except when data is mangled via the file structures).
   However, disk queues can be set to write bookkeeping information on checkpoints
   (every n records), so that this can be made ultra-reliable, too. If the
   checkpoint interval is set to one, no data can be lost, but the queue is
   exceptionally slow.
-  **queue.syncqueuefiles** on/off (default "off")

   Disk-based queues can be made very reliable by issuing a (f)sync after each
   write operation. This happens when you set the parameter to "on".
   Activating this option has a performance penalty, so it should not
   be turned on without a good reason. Note that the penalty also depends on
   *queue.checkpointInterval* frequency.

-  **queue.samplinginterval** number

   This option allows queues to be populated by events produced at a specific interval.
   It provides a way to sample data each N events, instead of processing all, in order to reduce resources usage (disk, bandwidth...)
   This feature is available for version 8.23 and above.

-  **queue.type** [FixedArray/LinkedList/**Direct**/Disk]
-  **queue.workerthreads** number
   number of worker threads, default 1, recommended 1
-  **queue.timeoutshutdown** number
   number is timeout in ms (1000ms is 1sec!), 0 means immediately
   default for action queues is 0, for rule set queues (including main queue) is 1500
-  **queue.timeoutactioncompletion** number
   number is timeout in ms (1000ms is 1sec!), default 1000, 0 means
   immediate!
-  **queue.timeoutenqueue** number
   number is timeout in ms (1000ms is 1sec!), default 2000, 0 means
   discard immediate.

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
     lost because the journal discards them (more agressive than rsyslog does)
   * if using imjournal, the journal will buffer messages. If journal
     runs out of configured space, messages will be discarded. So in this
     mode discarding is moved to a bit later place.
   * other non-delayable sources like imudp will also loose messages

   So this setting is provided in order to guard against problematic situations,
   which always will result either in message loss or system hang. For
   action queues, one may debate if it would be better to overflow rapidly
   to the main queue. If so desired, this is easy to acomplish by setting
   a very large timeout value. The same, of course, is true for the main
   queue, but you have been warned if you do so!

   In some other words, you can consider this scenario, using default values.
   With all progress blocked (unable to deliver a message):

   * all delayable inputs (tcp, relp, imfile, imjournal, etc) will block
     indefinantly (assuming queue.lightdelaymark and queue.fulldelaymark
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
-  **queue.timeoutworkerthreadshutdown** number
   number is timeout in ms (1000ms is 1sec!), default 60000 (1 minute)
-  **queue.workerthreadminimummessages** number
   default queue size/number of workers
-  **queue.maxfilesize** size\_nbr
   default 1m
-  **queue.saveonshutdown** on/\ **off**
-  **queue.dequeueslowdown** number
   number is timeout in microseconds (1000000us is 1sec!), default 0 (no
   delay). Simple rate-limiting!
-  **queue.dequeuetimebegin** number
-  **queue.dequeuetimeend** number
-  **queue.samplinginterval** number
   Sampling interval for action queue. This parameter specifies how many line
   of logs will be dropped before one enqueued. default 0.

**Sample:**

The following is a sample of a TCP forwarding action with its own queue.

::

  action(type="omfwd" target="192.168.2.11" port="10514" protocol="tcp"
         queue.filename="forwarding" queue.size="1000000" queue.type="LinkedList"
        )

