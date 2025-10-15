.. _param-impstats-log-file:
.. _impstats.parameter.module.log.file:

logfile
=======

.. index::
   single: impstats; logfile
   single: logfile

.. summary-start

Writes impstats statistics records to the specified local file in addition to other outputs.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/impstats`.

:Name: logfile
:Scope: module
:Type: word
:Default: module=none
:Required?: no
:Introduced: at least 5.x, possibly earlier

Description
-----------
If specified, statistics data is written to the specified file. For robustness,
this should be a local file. The file format cannot be customized; it consists
of a date header, followed by a colon, followed by the actual statistics
record, all on one line. Only very limited error handling is done, so if things
go wrong stats records will probably be lost. Logging to file can be a useful
alternative if for some reasons (e.g. full queues) the regular syslog stream
method shall not be used solely. Note that turning on file logging does NOT turn
off syslog logging. If that is desired ``logsyslog="off"`` must be explicitly
set.

Module usage
------------
.. _impstats.parameter.module.log.file-usage:

.. code-block:: rsyslog

   module(load="impstats" logFile="/var/log/rsyslog-stats")

See also
--------
See also :doc:`../../configuration/modules/impstats`.
