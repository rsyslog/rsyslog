.. _param-imuxsock-syssock-ratelimit-severity:
.. _imuxsock.parameter.module.syssock-ratelimit-severity:

SysSock.RateLimit.Severity
==========================

.. index::
   single: imuxsock; SysSock.RateLimit.Severity
   single: SysSock.RateLimit.Severity

.. summary-start

Specifies which message severity levels are rate-limited on the system log socket.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imuxsock`.

:Name: SysSock.RateLimit.Severity
:Scope: module
:Type: integer
:Default: module=1
:Required?: no
:Introduced: at least 7.0.0, possibly earlier

Description
-----------
Specifies the severity of messages that shall be rate-limited.

.. note::

   When ``syssock.ratelimit.name`` is used, configure the severity inside
   the referenced :doc:`ratelimit object </rainerscript/ratelimit>`.

.. seealso::

   https://en.wikipedia.org/wiki/Syslog#Severity_level

Module usage
------------
.. _param-imuxsock-module-syssock-ratelimit-severity:
.. _imuxsock.parameter.module.syssock-ratelimit-severity-usage:

.. code-block:: rsyslog

   module(load="imuxsock" sysSock.rateLimit.severity="5")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.
.. _imuxsock.parameter.legacy.systemlogratelimitseverity:

- $SystemLogRateLimitSeverity — maps to SysSock.RateLimit.Severity (status: legacy)

.. index::
   single: imuxsock; $SystemLogRateLimitSeverity
   single: $SystemLogRateLimitSeverity

See also
--------
See also :doc:`../../configuration/modules/imuxsock`.
