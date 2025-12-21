***********************************************************
impstats: Generate Periodic Statistics of Internal Counters
***********************************************************

===========================  ===========================================================================
**Module Name:**             **impstats**
**Author:**                  `Rainer Gerhards <https://rainer.gerhards.net/>`_ <rgerhards@adiscon.com>
===========================  ===========================================================================


Purpose
=======

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
Depending on settings, this impact may be noticeable for high-load
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
continuously being added, and older versions do not support everything.


Notable Features
================

- :ref:`impstats-statistic-counter`




Configuration Parameters
========================

The configuration parameters for this module are designed for tailoring
the method and process for outputting the rsyslog statistics to file.

.. note::

   Parameter names are case-insensitive; camelCase is recommended for improved readability.

.. note::

   This module supports module parameters, only.


Module Parameters
-----------------

.. list-table::
   :widths: 30 70
   :header-rows: 1

   * - Parameter
     - Summary
   * - :ref:`param-impstats-interval`
     - .. include:: ../../reference/parameters/impstats-interval.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-impstats-facility`
     - .. include:: ../../reference/parameters/impstats-facility.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-impstats-severity`
     - .. include:: ../../reference/parameters/impstats-severity.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-impstats-resetcounters`
     - .. include:: ../../reference/parameters/impstats-resetcounters.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-impstats-format`
     - .. include:: ../../reference/parameters/impstats-format.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-impstats-log-syslog`
     - .. include:: ../../reference/parameters/impstats-log-syslog.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-impstats-log-file`
     - .. include:: ../../reference/parameters/impstats-log-file.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-impstats-log-file-overwrite`
     - .. include:: ../../reference/parameters/impstats-log-file-overwrite.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-impstats-ruleset`
     - .. include:: ../../reference/parameters/impstats-ruleset.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-impstats-bracketing`
     - .. include:: ../../reference/parameters/impstats-bracketing.rst
        :start-after: .. summary-start
        :end-before: .. summary-end


.. _impstats-statistic-counter:

Statistic Counter
=================

The impstats plugin gathers some internal :doc:`statistics <../rsyslog_statistic_counter>`.
They have different names depending on the actual statistics. Obviously, they do not
relate to the plugin itself but rather to a broader object – most notably the
rsyslog process itself. The "resource-usage" counter maintains process
statistics. They base on the getrusage() system call. The counters are
named like getrusage returned data members. So for details, looking them
up in "man getrusage" is highly recommended, especially as value may be
different depending on the platform. A getrusage() call is done immediately
before the counter is emitted. The following individual counters are
maintained:

-  ``utime`` - this is the user time in microseconds (thus the timeval structure combined)
-  ``stime`` - again, time given in microseconds
-  ``maxrss``
-  ``minflt``
-  ``majflt``
-  ``inblock``
-  ``outblock``
-  ``nvcsw``
-  ``nivcsw``
-  ``openfiles`` - number of file handles used by rsyslog; includes actual files, sockets and others

Format
------

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "legacy", "no", "none"

.. versionadded:: 8.16.0

Specifies the format of emitted stats messages. The default of
"legacy" is compatible with pre v6-rsyslog. The other options provide
support for structured formats (note the "cee" is actually "project
lumberjack" logging).

The json-elasticsearch format supports the broken ElasticSearch
JSON implementation.  ES 2.0 no longer supports valid JSON and
disallows dots inside names.  The "json-elasticsearch" format
option replaces those dots by the bang ("!") character. So
"discarded.full" becomes "discarded!full".

The zabbix format supports arrayed-JSON objects with a single JSON object
per pstats emission to disk or syslog. This format should be compatible
with most JSON parsers from other monitoring products. log.file is highly
recommended as log.syslog may encounter message truncation problems if the
emission is large. If you must use log.syslog, it's recommended to monitor
pstats for truncation and increase $MaxMessageSize at the top of your
main rsyslog configuration file.

Options: json/json-elasticsearch/cee/legacy/zabbix


Caveats/Known Bugs
==================

-  This module MUST be loaded right at the top of rsyslog.conf,
   otherwise stats may not get turned on in all places.
-  When using the format "zabbix", it is not recommended to use
   logSyslog="on". This can cause message truncation in stats.


Examples
========

Load module, send stats data to syslog stream
---------------------------------------------

This activates the module and records messages to /var/log/rsyslog-stats
in 10 minute intervals:

.. code-block:: rsyslog

   module(load="impstats"
          interval="600"
          severity="7")

   # to actually gather the data:
   syslog.=debug /var/log/rsyslog-stats

Load module, format the output to Zabbix, and output to a local file
--------------------------------------------------------------------

.. code-block:: rsyslog

   module(loadf="impstats"
   interval="60"
   severity="7"
   format="zabbix"
   logSyslog="off"
   logFile="/var/log/stats.log")


Load module, send stats data to local file
------------------------------------------

Here, the default interval of 5 minutes is used. However, this time, stats
data is NOT emitted to the syslog stream but to a local file instead.

.. code-block:: rsyslog

   module(load="impstats"
          interval="600"
          severity="7"
          logSyslog="off"
          # need to turn log stream logging off!
          logFile="/path/to/local/stats.log")


Load module, send stats data to local file and syslog stream
------------------------------------------------------------

Here we log to both the regular syslog log stream as well as a
file. Within the log stream, we forward the data records to another
server:

.. code-block:: rsyslog

   module(load="impstats"
          interval="600"
          severity="7"
          logFile="/path/to/local/stats.log")

   syslog.=debug @central.example.net


Explanation of output
=====================

Example output for illustration::

   Sep 17 11:43:49 localhost rsyslogd-pstats: imuxsock: submitted=16
   Sep 17 11:43:49 localhost rsyslogd-pstats: main Q: size=1 enqueued=2403 full=0 maxqsize=2

Explanation:

All objects are shown in the results with a separate counter, one object per
line.

Line 1: shows details for

- ``imuxsock``, an object
- ``submitted=16``, a counter showing that 16 messages were received by the
  imuxsock object.

Line 2: shows details for the main queue:

- ``main Q``, an object
- ``size``, messages in the queue
- ``enqueued``, all received messages thus far
- ``full``, how often was the queue was full
- ``maxqsize``, the maximum amount of messages that have passed through the
  queue since rsyslog was started

See Also
========

-  `rsyslog statistics
   counter <http://www.rsyslog.com/rsyslog-statistic-counter/>`_
-  `impstats delayed or
   lost <http://www.rsyslog.com/impstats-delayed-or-lost/>`_ - cause and
   cure

.. toctree::
   :hidden:

   ../../reference/parameters/impstats-interval
   ../../reference/parameters/impstats-facility
   ../../reference/parameters/impstats-severity
   ../../reference/parameters/impstats-resetcounters
   ../../reference/parameters/impstats-format
   ../../reference/parameters/impstats-log-syslog
   ../../reference/parameters/impstats-log-file
   ../../reference/parameters/impstats-log-file-overwrite
   ../../reference/parameters/impstats-ruleset
   ../../reference/parameters/impstats-bracketing
