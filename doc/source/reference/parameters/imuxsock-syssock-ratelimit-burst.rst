.. _param-imuxsock-syssock-ratelimit-burst:
.. _imuxsock.parameter.module.syssock-ratelimit-burst:

SysSock.RateLimit.Burst
=======================

.. index::
   single: imuxsock; SysSock.RateLimit.Burst
   single: SysSock.RateLimit.Burst

.. summary-start

Sets the rate-limiting burst size in messages for the system log socket.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imuxsock`.

:Name: SysSock.RateLimit.Burst
:Scope: module
:Type: integer
:Default: module=200
:Required?: no
:Introduced: 5.7.1

Description
-----------
Specifies the rate-limiting burst in number of messages. The maximum is
``(2^31)-1``.

Module usage
------------
.. _param-imuxsock-module-syssock-ratelimit-burst:
.. _imuxsock.parameter.module.syssock-ratelimit-burst-usage:

.. code-block:: rsyslog

   module(load="imuxsock" sysSock.rateLimit.burst="100")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.
.. _imuxsock.parameter.legacy.systemlogratelimitburst:

- $SystemLogRateLimitBurst — maps to SysSock.RateLimit.Burst (status: legacy)

.. index::
   single: imuxsock; $SystemLogRateLimitBurst
   single: $SystemLogRateLimitBurst

See also
--------
See also :doc:`../../configuration/modules/imuxsock`.
