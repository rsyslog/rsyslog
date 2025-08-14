.. _param-imtcp-ratelimit-burst:
.. _imtcp.parameter.input.ratelimit-burst:

RateLimit.Burst
===============

.. index::
   single: imtcp; RateLimit.Burst
   single: RateLimit.Burst

.. summary-start

Specifies the rate-limiting burst in number of messages.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imtcp`.

:Name: RateLimit.Burst
:Scope: input
:Type: integer
:Default: 10000
:Required?: no
:Introduced: at least 5.x, possibly earlier

Description
-----------
This parameter specifies the rate-limiting burst in number of messages. It defines how many messages can be received in a burst before rate-limiting is applied. This parameter is only effective if ``rateLimit.interval`` is also set.

Input usage
-----------
.. _imtcp.parameter.input.ratelimit-burst-usage:

.. code-block:: rsyslog

   input(type="imtcp"
         port="514"
         rateLimit.interval="5"
         rateLimit.burst="20000")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.

This parameter has no legacy name.

See also
--------
See also :doc:`../../configuration/modules/imtcp`.
