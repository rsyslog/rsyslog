General Queue Parameters
------------------------

Queue parameters can be used together with the following statements:

-  `action() <rsyslog_conf_actions.html>`_
-  ruleset()
-  main\_queue()

Queues need to be configured in the action or ruleset it should affect.
If nothing is configured, default values will be used. Thus, the default
ruleset has only the default main queue. Specific Action queues are not
set up by default.

-  **queue.filename** name
   File name to be used for the queue files. Please note that this is
   actually just the file name. A directory can NOT be specified in this
   paramter. If the files shall be created in a specific directory,
   specify queue.spoolDirectory for this. The filename is used to build
   to complete path for queue files.
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
-  **queue.dequeuebatchsize** number
   default 16
-  **queue.maxdiskspace** number
   The maximum size that all queue files together will use on disk. Note
   that the actual size may be slightly larger than the configured max,
   as rsyslog never writes partial queue records.
-  **queue.highwatermark** number
   This applies to disk-assisted queues, only. When the queue fills up
   to this number of messages, the queue begins to spool messages to
   disk. Please note that this should note happen as part of usual
   processing, because disk queue mode is very considerably slower than
   in-memory queue mode. Going to disk should be reserved for cases
   where an output action destination is offline for some period.
-  **queue.lowwatermark** number
   default 2000
-  **queue.fulldelaymark** number 
   Number of messages when the queue should block delayable messages. 
   Messages are NO LONGER PROCESSED until the queue has sufficient space 
   again. If a message is delayable depends on the input. For example, 
   messages received via imtcp are delayable (because TCP can push back), 
   but those received via imudp are not (as UDP does not permit a push back).
   The intent behind this setting is to leave some space in an almost-full 
   queue for non-delayable messages, which would be lost if the queue runs 
   out of space. Please note that if you use a DA queue, setting the 
   fulldelaymark ABOVE the highwatermark makes the queue never activate 
   disk mode for delayable inputs. So this is probably not what you want.
-  **queue.lightdelaymark** number
-  **queue.discardmark** number
   default 9750]
-  **queue.discardseverity** number
   \*numerical\* severity! default 8 (nothing discarded)
-  **queue.checkpointinterval** number
   Disk queues by default do not update housekeeping structures every time 
   it writes to disk. This is for performance reasons. In the event of failure, 
   data will still be lost (except when data is mangled via the file structures).
   However, disk queues can be set to write bookkeeping information on checkpoints 
   (every n records), so that this can be made ultra-reliable, too. If the 
   checkpoint interval is set to one, no data can be lost, but the queue is 
   exceptionally slow.
-  **queue.syncqueuefiles** on/off 
   Disk-based queues can be made very reliable by issuing a (f)sync after each 
   write operation. Starting with version 4.3.2, this can be requested via 
   "<object>QueueSyncQueueFiles on/off with the default being off. Activating 
   this option has a performance penalty, so it should not be turned on without 
   reason.
-  **queue.type** [FixedArray/LinkedList/**Direct**/Disk]
-  **queue.workerthreads** number
   number of worker threads, default 1, recommended 1
-  **queue.timeoutshutdown** number
   number is timeout in ms (1000ms is 1sec!), default 0 (indefinite)
-  **queue.timeoutactioncompletion** number
   number is timeout in ms (1000ms is 1sec!), default 1000, 0 means
   immediate!
-  **queue.timeoutenqueue** number
   number is timeout in ms (1000ms is 1sec!), default 2000, 0 means
   indefinite
-  **queue.timeoutworkerthreadshutdown** number
   number is timeout in ms (1000ms is 1sec!), default 60000 (1 minute)
-  **queue.workerthreadminimummessages** number
   default 100
-  **queue.maxfilesize** size\_nbr
   default 1m
-  **queue.saveonshutdown** on/\ **off**
-  **queue.dequeueslowdown** number
   number is timeout in microseconds (1000000us is 1sec!), default 0 (no
   delay). Simple rate-limiting!
-  **queue.dequeuetimebegin** number
-  **queue.dequeuetimeend** number

**Sample:**

The following is a sample of a TCP forwarding action with its own queue.

::

  action(type="omfwd" target="192.168.2.11" port="10514" protocol="tcp"
         queue.filename="forwarding" queue.size="1000000" queue.type="LinkedList"
        )

This documentation is part of the `rsyslog <http://www.rsyslog.com/>`_
project.
Copyright © 2013-2014 by `Rainer Gerhards <http://www.gerhards.net/rainer>`_
and `Adiscon <http://www.adiscon.com/>`_. Released under the GNU GPL
version 3 or higher.
