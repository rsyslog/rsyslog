.. _param-imudp-ratelimit-interval:
.. _imudp.parameter.module.ratelimit-interval:

RateLimit.Interval
==================

.. index::
   single: imudp; RateLimit.Interval
   single: RateLimit.Interval

.. summary-start

Sets the interval in seconds used for input rate limiting.

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
Defines the time window for rate limiting; a value of `0` disables the limit while positive values enable throttling.

Input usage
-----------
.. _param-imudp-input-ratelimit-interval:
.. _imudp.parameter.input.ratelimit-interval:
.. code-block:: rsyslog

   input(type="imudp" RateLimit.Interval="...")

Notes
-----
.. versionadded:: 7.3.1

See also
--------
See also :doc:`../../configuration/modules/imudp`.
