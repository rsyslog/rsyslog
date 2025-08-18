.. _param-imuxsock-ratelimit-interval:
.. _imuxsock.parameter.input.ratelimit-interval:

RateLimit.Interval
==================

.. index::
   single: imuxsock; RateLimit.Interval
   single: RateLimit.Interval

.. summary-start

Sets the rate-limiting interval in seconds for this input.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imuxsock`.

:Name: RateLimit.Interval
:Scope: input
:Type: integer
:Default: input=0
:Required?: no
:Introduced: at least 7.0.0, possibly earlier

Description
-----------
Specifies the rate-limiting interval in seconds. Default value is 0,
which turns off rate limiting. Set it to a number of seconds (5
recommended) to activate rate-limiting. The default of 0 has been
chosen as people experienced problems with this feature activated
by default. Now it needs an explicit opt-in by setting this parameter.

Input usage
-----------
.. _param-imuxsock-input-ratelimit-interval:
.. _imuxsock.parameter.input.ratelimit-interval-usage:

.. code-block:: rsyslog

   input(type="imuxsock" rateLimit.interval="5")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.
.. _imuxsock.parameter.legacy.imuxsockratelimitinterval:

- $IMUXSockRateLimitInterval — maps to RateLimit.Interval (status: legacy)

.. index::
   single: imuxsock; $IMUXSockRateLimitInterval
   single: $IMUXSockRateLimitInterval

See also
--------
See also :doc:`../../configuration/modules/imuxsock`.
