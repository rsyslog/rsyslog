.. _param-impstats-log-syslog:
.. _impstats.parameter.module.log-syslog:

log.syslog
==========

.. index::
   single: impstats; log.syslog
   single: log.syslog

.. summary-start

Enables or disables sending statistics to the regular syslog stream.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/impstats`.

:Name: log.syslog
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
an extended period of time in queues if they are full). If set ``off``, then you
cannot bind the module to ``ruleset``.

Module usage
------------
.. _param-impstats-module-log-syslog-usage:
.. _impstats.parameter.module.log-syslog-usage:

.. code-block:: rsyslog

   module(load="impstats" log.syslog="off")

See also
--------
See also :doc:`../../configuration/modules/impstats`.
