.. meta::
   :description: Troubleshoot rsyslog apparent hangs when queues fill because an output is slow or unavailable.
   :keywords: rsyslog, troubleshooting, queue full, timeoutEnqueue, disk-assisted queue, discardMark, impstats, flow control

.. _troubleshooting_queue_full:

Rsyslog Appears to Hang When Queues Fill
========================================

.. summary-start

When an output action is slower than the incoming log rate, rsyslog queues can
fill. A full main or ruleset queue can make inputs slow down while they wait for
space, which can look like rsyslog or local applications are hanging.

.. summary-end

Why this happens
----------------

Rsyslog uses queues to decouple inputs, rulesets, and actions. If a destination
such as a remote server, database, or helper program is unavailable, the
affected queue may grow until it reaches its configured limits. At that point
rsyslog applies flow control instead of accepting unlimited messages.

For the main message queue and ruleset queues, this can affect inputs. Local
applications that write to a blocking log socket may then also slow down because
rsyslog is no longer draining that socket quickly. This is usually not a deadlock
inside rsyslog; it is backpressure caused by a downstream queue that cannot make
progress.

The key setting is ``queue.timeoutEnqueue``. When a queue is full, rsyslog waits
up to that timeout for space to become available. The default avoids immediately
dropping messages, but a large value on the main queue can make the whole system
feel stuck under sustained pressure. A value of ``0`` discards immediately when
the queue cannot accept a message.

Confirm the condition
---------------------

Enable :doc:`impstats <../configuration/modules/impstats>` and inspect the
queue counters while the problem is happening:

* ``size`` near ``queue.size`` shows the queue is full or nearly full.
* ``full`` increases when the queue cannot accept more messages.
* ``discarded.full`` increases when messages are dropped because the queue is
  full.
* ``discarded.nf`` increases when the discard watermark is active.
* Action counters with ``suspended`` or repeated failures identify the slow or
  unavailable destination.

Also check rsyslog's own diagnostics for messages about suspended actions,
queue full conditions, disk queue problems, or failed output destinations.

Reduce the blast radius
-----------------------

Prefer isolating slow destinations behind dedicated action queues. For example,
a remote forwarding action can have its own disk-assisted queue so local file
outputs and unrelated actions continue while the remote target is offline:

.. code-block:: rsyslog

   global(workDirectory="/var/spool/rsyslog")

   action(
       type="omfwd"
       target="loghost.example.net"
       protocol="tcp"
       queue.type="LinkedList"
       queue.filename="fwd"
       queue.maxDiskSpace="2g"
       queue.saveOnShutdown="on"
       action.resumeRetryCount="-1"
   )

For the main queue, avoid very large ``queue.timeoutEnqueue`` values unless the
host is intentionally allowed to slow log producers instead of losing messages.
Choose an explicit policy for overload:

* buffer temporarily with enough disk space and monitoring;
* discard lower-priority messages with ``queue.discardMark`` and
  ``queue.discardSeverity``;
* discard immediately on a full queue with ``queue.timeoutEnqueue="0"``;
* increase queue size only when memory and disk capacity are understood.

Operational checklist
---------------------

When diagnosing this symptom:

* verify the unavailable or slow action first;
* enable impstats and alert on queue ``size``, ``full``, and discard counters;
* use per-action queues for remote, database, or helper-program outputs;
* set ``queue.maxDiskSpace`` on disk-assisted queues so spool files cannot fill
  the filesystem silently;
* decide up front whether the system should prefer blocking, buffering, or
  dropping during sustained overload.

See also
--------

* :doc:`Understanding rsyslog queues <../concepts/queues>`
* :doc:`Queue parameters <../rainerscript/queue_parameters>`
* :doc:`Action configuration <../configuration/actions>`
* :doc:`impstats counters <../configuration/rsyslog_statistic_counter>`
