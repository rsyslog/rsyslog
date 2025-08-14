.. _param-imudp-ratelimit-interval:
.. _imudp.parameter.module.ratelimit-interval:

RateLimit.Interval
==================

.. index::
   single: imudp; RateLimit.Interval
   single: RateLimit.Interval

.. summary-start

Length of the rate-limiting window in seconds; ``0`` disables rate limiting.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imudp`.

:Name: RateLimit.Interval
:Scope: input
:Type: integer
:Default: input=0
:Required?: no
:Introduced: 7.3.1

Description
-----------
Defines the interval in seconds used for rate limiting. A value of ``0`` turns
off rate limiting; setting it to a positive value such as ``5`` activates the
mechanism.

Input usage
-----------
.. _param-imudp-input-ratelimit-interval:
.. _imudp.parameter.input.ratelimit-interval:
.. code-block:: rsyslog

   input(type="imudp" RateLimit.Interval="...")

See also
--------
See also :doc:`../../configuration/modules/imudp`.

