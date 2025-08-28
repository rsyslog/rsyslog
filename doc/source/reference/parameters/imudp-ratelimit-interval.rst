.. _param-imudp-ratelimit-interval:
.. _imudp.parameter.input.ratelimit-interval:

RateLimit.Interval
==================

.. index::
   single: imudp; RateLimit.Interval
   single: RateLimit.Interval

.. summary-start

Rate-limiting interval in seconds; ``0`` disables throttling.

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
The rate-limiting interval in seconds. Value 0 turns off rate limiting. Set it to
a number of seconds (5 recommended) to activate rate-limiting.

Input usage
-----------
.. _param-imudp-input-ratelimit-interval:
.. _imudp.parameter.input.ratelimit-interval-usage:

.. code-block:: rsyslog

   input(type="imudp" RateLimit.Interval="...")

See also
--------
See also :doc:`../../configuration/modules/imudp`.
