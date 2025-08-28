.. _param-imtcp-ratelimit-interval:
.. _imtcp.parameter.input.ratelimit-interval:

RateLimit.Interval
==================

.. index::
   single: imtcp; RateLimit.Interval
   single: RateLimit.Interval

.. summary-start

Sets the rate-limiting interval in seconds.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imtcp`.

:Name: RateLimit.Interval
:Scope: input
:Type: integer
:Default: input=0
:Required?: no
:Introduced: at least 5.x, possibly earlier

Description
-----------
Specifies the rate-limiting interval in seconds. Default value is ``0``, which turns off rate
limiting. Set it to a number of seconds (``5`` recommended) to activate rate limiting.

Input usage
-----------
.. _param-imtcp-input-ratelimit-interval:
.. _imtcp.parameter.input.ratelimit-interval-usage:

.. code-block:: rsyslog

   input(type="imtcp" rateLimit.Interval="5")

See also
--------
See also :doc:`../../configuration/modules/imtcp`.
