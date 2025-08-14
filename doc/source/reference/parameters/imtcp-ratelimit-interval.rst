.. _param-imtcp-ratelimit-interval:
.. _imtcp.parameter.input.ratelimit-interval:

RateLimit.Interval
==================

.. index::
   single: imtcp; RateLimit.Interval
   single: RateLimit.Interval

.. summary-start

Specifies the rate-limiting interval in seconds.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imtcp`.

:Name: RateLimit.Interval
:Scope: input
:Type: integer
:Default: 0
:Required?: no
:Introduced: at least 5.x, possibly earlier

Description
-----------
This parameter specifies the rate-limiting interval in seconds. A value of ``0`` (the default) turns off rate limiting. Set it to a positive number of seconds (e.g., 5) to activate rate-limiting for this listener.

Input usage
-----------
.. _imtcp.parameter.input.ratelimit-interval-usage:

.. code-block:: rsyslog

   input(type="imtcp" port="514" rateLimit.interval="5")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.

This parameter has no legacy name.

See also
--------
See also :doc:`../../configuration/modules/imtcp`.
