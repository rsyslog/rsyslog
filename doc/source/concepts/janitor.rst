The Janitor Process
===================
The janitor process carries out periodic cleanup tasks. For example,
it is used by
:doc:`omfile <../configuration/modules/omfile>`
to close files after a timeout has expired.

The janitor runs periodically. As such, all tasks carried out via the
janitor will be activated based on the interval at which it runs. This
means that all janitor-related times set are approximate and should be
considered as "no earlier than" (NET). If, for example, you set a timeout
to 5 minutes and the janitor is run in 10-minute intervals, the timeout
may actually happen after 5 minutes, but it may also take up to 20
minutes for it to be detected.

In general (see note about HUP below), janitor based activities scheduled
to occur after *n* minutes will occur after *n* and *(n + 2\*janitorInterval)*
minutes.

To reduce the potential delay caused by janitor invocation,
:ref:`the interval at which the janitor runs can be adjusted <global_janitorInterval>`\ .
If high precision is
required, it should be set to one minute. Janitor-based activities will
still be NET times, but the time frame will be much smaller. In the
example with the file timeout, it would be between 5 and 6 minutes if the
janitor is run at a one-minute interval.

Note that the more frequent the janitor is run, the more frequent the
system needs to wakeup from potential low power state. This is no issue
for data center machines (which usually always run at full speed), but it
may be an issue for power-constrained environments like notebooks. For
such systems, a higher janitor interval may make sense.

As a special case, sending a HUP signal to rsyslog also activates the
janitor process. This can lead to too-frequent wakeups of janitor-related
services. However, we don't expect this to cause any issues. If it does,
it could be solved by creating a separate thread for the janitor. But as
this takes up some system resources and is not considered useful, we
have not implemented it that way. If the HUP/janitor interaction causes
problems, let the rsyslog team know and we can change the implementation.


.. _concept-model-concepts-janitor:

Conceptual model
----------------

- The janitor is a periodic scheduler that runs maintenance jobs such as closing inactive files.
- All time-based actions are NET (no earlier than) relative to the janitor interval, so deadlines are approximate windows.
- Frequency tuning trades precision for power usage: shorter intervals reduce latency but increase wakeups.
- A HUP signal triggers an immediate janitor run, potentially bunching cleanup work with configuration reloads.
- Janitor work currently reuses existing threads; a dedicated thread is avoided to conserve resources unless needed.

