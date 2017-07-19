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

.. toctree::
   :maxdepth: 1

   modules/imuxsock
   modules/imudp
   modules/imtcp
   modules/imptcp
   modules/imrelp
   modules/impstats
   modules/omfile
   modules/omelasticsearch


