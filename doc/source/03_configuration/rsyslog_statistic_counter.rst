rsyslog statistic counter
=========================

Rsyslog supports statistic counters via the :doc:`impstats <modules/impstats>` module.
It is important to know that impstats and friends only provides an infrastructure
where core components and plugins can register statistics counter. This FAQ entry
tries to describe all counters available, but please keep in mind that there may exist
that we  do not know about.

When interpreting rsyslog statistics, please keep in mind that statistics records are
processed as regular syslog messages. As such, the statistics messages themselves
increment counters when they are emitted via the regular syslog stream, which is the
default (and so counters keep slowly increasing even if there is absolutely no other
traffic). Also keep in mind that a busy rsyslog system is very dynamic. Most
importantly, this means that the counters may not be 100% consistent, but some slight
differences may exist. Avoiding such inconsistencies would be possible only at the
price of a very tight locking discipline, which would cause serious performance
bottlenecks. Thus, this is not done. Finally, though extremely unlikely, some counters
may experience an overflow and restart at 0 for that reasons. However, most counters
are 64-bit, so this is extremely unlikely. Those which are not 64 bit are typically
taken from some internal data structure that uses lower bits for performance reasons
and guards against overflow.

The listing starts with the core component or plugin that creates the counters and
than specifies various counters that exist for the sub-entities. The listing below is
extended as new counters are added. Some counters probably do not exist in older
releases of rsyslog.

Queue
-----

For each queue inside the system its own set of statistics counters is created.
If there are multiple action (or main) queues, this can become a rather lengthy list.
The stats record begins with the queue name (e.g. "main Q" for the main queue;
ruleset queues have the name of the ruleset they are associated to, action queues
the name of the action).

-  **size** - currently active messages in queue

-  **enqueued** - total number of messages enqueued into this queue since startup

-  **maxsize** - maximum number of active messages the queue ever held

-  **full** - number of times the queue was actually full and could not accept additional messages

-  **discarded.full** - number of messages discarded because the queue was full

-  **discarded.nf** - number of messages discarded because the queue was nearly full. Starting at this point, messages of lower-than-configured severity are discarded to save space for higher severity ones.

Actions
-------

-  **processed** - total number of messages processed by this action. This includes those messages that failed actual execution (so it is a total count of messages ever seen, but not necessarily successfully processed)

-  **failed** - total number of messages that failed during processing. These are actually lost if they have not been processed by some other action. Most importantly in a failover chain the messages are flagged as "failed" in the failing actions even though they are forwarded to the failover action (the failover action’s "processed" count should equal to failing actions "fail" count in this scenario)a

-  **suspended** - (7.5.8+) – total number of times this action suspended itself. Note that this counts the number of times the action transitioned from active to suspended state. The counter is no indication of how long the action was suspended or how often it was retried. This is intentional, as the counter as it currently is permits to tell how often the action ran into a failure condition.

-  **suspended.duration** - (7.5.8+) – the total number of seconds this action was disabled. Note that the second count is incremented as soon as the action is suspended for another interval. As such, the time may be higher than it should be at the reporting point of time. If the pstats interval is shorter than the suspension timeout, the same suspended.duration value may be reported for successive pstats outputs. For a long-running system, this is considered a minimal difference. In general, note that this setting is not totally accurate, especially when running with multiple worker threads. In rsyslog v8, this is the total suspended time for all worker instances of this action.

-  **resumed** - (7.5.8+) – total number of times this action resumed itself. A resumption occurs after the action has detected that a failure condition does no longer exist.

Plugins
-------

:doc:`imuxsock <modules/imuxsock>`

:doc:`imudp <modules/imudp>`

:doc:`imtcp <modules/imtcp>`

:doc:`imptcp <modules/imptcp>`

:doc:`imrelp <modules/imrelp>`

:doc:`impstats <modules/impstats>`

:doc:`omfile <modules/omfile>`

:doc:`omelasticsearch <modules/omelasticsearch>`

:doc:`omkafka <modules/omkafka>`

