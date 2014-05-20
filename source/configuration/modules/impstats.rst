impstats: Generate Periodic Statistics of Internal Counters
===========================================================

**Author:**\ Rainer Gerhards <rgerhards@adiscon.com>

This module provides periodic output of rsyslog internal counters.

The set of available counters will be output as a set of syslog
messages. This output is periodic, with the interval being configurable
(default is 5 minutes). Be sure that your configuration records the
counter messages (default is syslog.=info). Besides logging to the
regular syslog stream, the module can also be configured to write
statistics data into a (local) file.

When logging to the regular syslog stream, impstats records are emitted
just like regular log messages. As such,
counters increase when processing these messages. This must be taken into
consideration when testing and troubleshooting.

Note that loading this module has some impact on rsyslog performance.
Depending on settings, this impact may be noticable for high-load
environments, but in general the overhead is pretty light.

**Note that there is a** `rsyslog statistics online
analyzer <http://www.rsyslog.com/impstats-analyzer/>`_ **available.** It
can be given a impstats-generated file and will return problems it
detects. Note that the analyzer cannot replace a human in getting things
right, but it is expected to be a good aid in starting to understand and
gain information from the pstats logs.

The rsyslog website has an overview of available `rsyslog
statistic counters <http://rsyslog.com/rsyslog-statistic-counter/>`_. 
When browsing this page, please be sure to take note of which rsyslog
version is required to provide a specific counter. Counters are 
continously being added, and older versions do not support everything.


Configuration Directives
------------------------

The configuration directives for this module are designed for tailoring
the method and process for outputting the rsyslog statistics to file.

Module Confguration Parameters
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

This module supports module parameters, only.

.. function:: interval\ [seconds] 

   *Default 300 [5minutes]*

   Sets the interval, in **seconds** at which messages are generated.
   Please note that the actual interval may be a bit longer. We do not
   try to be precise and so the interval is actually a sleep period
   which is entered after generating all messages. So the actual
   interval is what is configured here plus the actual time required to
   generate messages. In general, the difference should not really
   matter.

.. function:: facility\ [facility number]

   The numerical syslog facility code to be used for generated
   messages. Default is 5 (syslog). This is useful for filtering
   messages.

.. function:: severity\ [severity number]

   The numerical syslog severity code to be used for generated
   messages. Default is 6 (info).This is useful for filtering messages.

.. function:: resetCounters\ off/on

   *Defaults to off*

   When set to "on", counters are automatically reset after they are
   emitted. In that case, the contain only deltas to the last value
   emitted. When set to "off", counters always accumulate their values.
   Note that in auto-reset mode not all counters can be reset. Some
   counters (like queue size) are directly obtained from internal object
   and cannot be modified. Also, auto-resetting introduces some
   additional slight inaccuracies due to the multi-threaded nature of
   rsyslog and the fact that for performance reasons it cannot serialize
   access to counter variables. As an alternative to auto-reset mode,
   you can use rsyslog's statistics manipulation scripts to create delta
   values from the regular statistic logs. This is the suggested method
   if deltas are not necessarily needed in real-time.

.. function:: format\ json/cee/legacy

   *Defaults to legacy*

   Specifies the format of emitted stats messages. The default of
   "legacy" is compatible with pre v6-rsyslog. The other options provide
   support for structured formats (note the "cee" is actually "project
   lumberack" logging).

.. function:: log.syslog\ on/off

   *Defaults to on*

   This is a boolean setting specifying if data should be sent to the
   usual syslog stream. This is useful if custom formatting or more
   elaborate processing is desired. However, output is placed under the
   same restrictions as regular syslog data, especially in regard to the
   queue position (stats data may sit for an extended period of time in
   queues if they are full).

.. function:: log.file\ [file name]

   If specified, statistics data is written the specified file. For
   robustness, this should be a local file. The file format cannot be
   customized, it consists of a date header, followed by a colon,
   followed by the actual statistics record, all on one line. Only very
   limited error handling is done, so if things go wrong stats records
   will probably be lost. Logging to file an be a useful alternative if
   for some reasons (e.g. full queues) the regular syslog stream method
   shall not be used solely. Note that turning on file logging does NOT
   turn off syslog logging. If that is desired log.syslog="off" must be
   explicitely set.

.. function:: Ruleset [ruleset]

   Binds the listener to a specific :doc:`ruleset <../../concepts/multi_ruleset>`.

Legacy Configuration Directives
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

A limited set of parameters can also be set via the legacy configuration
syntax. Note that this is intended as an upward compatibilit layer, so
newer features are intentionally **not** available via legacy
directives.

-  $PStatInterval <Seconds> - same as the "interval" parameter.
-  $PStatFacility <numerical facility> - same as the "facility"
   parameter.
-  $PStatSeverity <numerical severity> - same as the "severity"
   parameter.
-  $PStatJSON <on/**off**> (rsyslog v6.3.8+ only)
   If set to on, stats messages are emitted as structured cee-enhanced
   syslog. If set to off, legacy format is used (which is compatible
   with pre v6-rsyslog).

Caveats/Known Bugs
------------------

-  This module MUST be loaded right at the top of rsyslog.conf,
   otherwise stats may not get turned on in all places.

Example
-------

This activates the module and records messages to /var/log/rsyslog-stats
in 10 minute intervals:

::

  module(load="impstats" 
         interval="600" 
         severity="7")
  
  # to actually gather the data: 
  syslog.=debug /var/log/rsyslog-stats

In the next sample, the default interval of 5 minutes is used. However,
this time stats data is NOT emitted to the syslog stream but to a local
file instead.

::

  module(load="impstats"
         interval="600"
         severity="7"
         log.syslog="off"
         /\* need to turn log stream logging off! \*/
         log.file="/path/to/local/stats.log")

And finally, we log to both the regular syslog log stream as well as a
file. Within the log stream, we forward the data records to another
server:

::

  module(load="impstats"
         interval="600"
         severity="7"
          log.file="/path/to/local/stats.log")

  syslog.=debug @central.example.net

Legacy Sample
-------------

This activates the module and records messages to /var/log/rsyslog-stats
in 10 minute intervals:

::

  $ModLoad impstats
  $PStatInterval 600
  $PStatSeverity 7
  syslog.=debug /var/log/rsyslog-stats

See Also
--------

-  `rsyslog statistics
   counter <http://www.rsyslog.com/rsyslog-statistic-counter/>`_
-  `impstats delayed or
   lost <http://www.rsyslog.com/impstats-delayed-or-lost/>`_ - cause and
   cure

