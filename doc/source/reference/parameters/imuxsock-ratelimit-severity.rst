.. _param-imuxsock-ratelimit-severity:
.. _imuxsock.parameter.input.ratelimit-severity:

RateLimit.Severity
==================

.. index::
   single: imuxsock; RateLimit.Severity
   single: RateLimit.Severity

.. summary-start

Defines the severity level at or below which messages are rate-limited.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imuxsock`.

:Name: RateLimit.Severity
:Scope: input
:Type: integer
:Default: input=1
:Required?: no
:Introduced: at least 7.0.0, possibly earlier

Description
-----------
Specifies the severity of messages that shall be rate-limited.

.. note::

   When ``ratelimit.name`` is used, configure the severity inside the
   referenced :doc:`ratelimit object </rainerscript/ratelimit>` instead of
   setting this parameter explicitly.

.. seealso::

   https://en.wikipedia.org/wiki/Syslog#Severity_level

Input usage
-----------
.. _param-imuxsock-input-ratelimit-severity:
.. _imuxsock.parameter.input.ratelimit-severity-usage:

.. code-block:: rsyslog

   input(type="imuxsock" rateLimit.severity="5")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.
.. _imuxsock.parameter.legacy.imuxsockratelimitseverity:

- $IMUXSockRateLimitSeverity — maps to RateLimit.Severity (status: legacy)

.. index::
   single: imuxsock; $IMUXSockRateLimitSeverity
   single: $IMUXSockRateLimitSeverity

See also
--------
See also :doc:`../../configuration/modules/imuxsock`.
