.. _param-imuxsock-ratelimit-burst:
.. _imuxsock.parameter.input.ratelimit-burst:

RateLimit.Burst
===============

.. index::
   single: imuxsock; RateLimit.Burst
   single: RateLimit.Burst

.. summary-start

Sets the rate-limiting burst size in messages for this input.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imuxsock`.

:Name: RateLimit.Burst
:Scope: input
:Type: integer
:Default: input=200
:Required?: no
:Introduced: 5.7.1

Description
-----------
Specifies the rate-limiting burst in number of messages. The maximum is
``(2^31)-1``.

Input usage
-----------
.. _param-imuxsock-input-ratelimit-burst:
.. _imuxsock.parameter.input.ratelimit-burst-usage:

.. code-block:: rsyslog

   input(type="imuxsock" rateLimit.burst="500")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.
.. _imuxsock.parameter.legacy.imuxsockratelimitburst:

- $IMUXSockRateLimitBurst — maps to RateLimit.Burst (status: legacy)

.. index::
   single: imuxsock; $IMUXSockRateLimitBurst
   single: $IMUXSockRateLimitBurst

See also
--------
See also :doc:`../../configuration/modules/imuxsock`.
