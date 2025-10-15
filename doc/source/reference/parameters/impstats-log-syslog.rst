.. _param-impstats-log-syslog:
.. _impstats.parameter.module.log.syslog:

logsyslog
=========

.. index::
   single: impstats; logsyslog
   single: logsyslog

.. summary-start

Enables or disables sending statistics to the regular syslog stream.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/impstats`.

:Name: logsyslog
:Scope: module
:Type: boolean
:Default: module=on
:Required?: no
:Introduced: at least 5.x, possibly earlier

Description
-----------
This is a boolean setting specifying if data should be sent to the usual syslog
stream. This is useful if custom formatting or more elaborate processing is
desired. However, output is placed under the same restrictions as regular
syslog data, especially in regard to the queue position (stats data may sit for
an extended period of time in queues if they are full). If set to ``off``, the
``ruleset`` parameter cannot be used, as syslog stream processing is required
for rulesets.

Module usage
------------
.. _impstats.parameter.module.log.syslog-usage:

.. code-block:: rsyslog

   module(load="impstats" logSyslog="off")

See also
--------
See also :doc:`../../configuration/modules/impstats`.
