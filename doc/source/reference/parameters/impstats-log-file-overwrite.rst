.. _param-impstats-log-file-overwrite:
.. _impstats.parameter.module.log-file-overwrite:

log.file.overwrite
==================

.. index::
   single: impstats; log.file.overwrite
   single: log.file.overwrite

.. summary-start

If set to "on", the statistics log file specified by :ref:`param-impstats-log-file`
is overwritten with each emission instead of being appended to.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/impstats`.

:Name: log.file.overwrite
:Scope: module
:Type: binary
:Default: off
:Required?: no
:Introduced: 8.2602.0

Description
-----------
When this parameter is set to ``on``, rsyslog will overwrite the file specified
in ``log.file`` every time it emits statistics. This is useful for external
monitoring tools (like Prometheus sidecars or node exporter) that expect to
read a single, consistent set of metrics from a file.

To ensure that reader processes always see a complete and consistent set of
statistics, rsyslog writes the data to a temporary file first and then
atomically renames it to the final destination.

Note that this parameter only has an effect if ``log.file`` is also specified.

Module usage
------------
.. _impstats.parameter.module.log-file-overwrite-usage:

.. code-block:: rsyslog

   module(load="impstats" 
          logFile="/var/log/rsyslog-stats"
          log.file.overwrite="on")

See also
--------
See also :doc:`../../configuration/modules/impstats`.
