.. _param-imuxsock-syssock-ratelimit-interval:
.. _imuxsock.parameter.module.syssock-ratelimit-interval:

SysSock.RateLimit.Interval
==========================

.. index::
   single: imuxsock; SysSock.RateLimit.Interval
   single: SysSock.RateLimit.Interval

.. summary-start

Sets the rate-limiting interval in seconds for the system log socket.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imuxsock`.

:Name: SysSock.RateLimit.Interval
:Scope: module
:Type: integer
:Default: module=0
:Required?: no
:Introduced: at least 7.0.0, possibly earlier

Description
-----------
Specifies the rate-limiting interval in seconds. Default value is 0,
which turns off rate limiting. Set it to a number of seconds (5
recommended) to activate rate-limiting. The default of 0 has been
chosen as people experienced problems with this feature activated
by default. Now it needs an explicit opt-in by setting this parameter.

Module usage
------------
.. _param-imuxsock-module-syssock-ratelimit-interval:
.. _imuxsock.parameter.module.syssock-ratelimit-interval-usage:

.. code-block:: rsyslog

   module(load="imuxsock" sysSock.rateLimit.interval="5")

.. include:: ratelimit-inline-note.rst

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.
.. _imuxsock.parameter.legacy.systemlogratelimitinterval:

- $SystemLogRateLimitInterval — maps to SysSock.RateLimit.Interval (status: legacy)

.. index::
   single: imuxsock; $SystemLogRateLimitInterval
   single: $SystemLogRateLimitInterval

See also
--------
See also :doc:`../../configuration/modules/imuxsock`.
