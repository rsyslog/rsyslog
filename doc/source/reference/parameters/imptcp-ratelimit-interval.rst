.. _param-imptcp-ratelimit-interval:
.. _imptcp.parameter.input.ratelimit-interval:

RateLimit.Interval
==================

.. index::
   single: imptcp; RateLimit.Interval
   single: RateLimit.Interval

.. summary-start

Specifies the rate-limiting interval in seconds.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imptcp`.

:Name: RateLimit.Interval
:Scope: input
:Type: integer
:Default: input=0
:Required?: no
:Introduced: at least 5.x, possibly earlier

Description
-----------
Specifies the rate-limiting interval in seconds. Set it to a number
of seconds (5 recommended) to activate rate-limiting.

Input usage
-----------
.. _param-imptcp-input-ratelimit-interval:
.. _imptcp.parameter.input.ratelimit-interval-usage:

.. code-block:: rsyslog

   input(type="imptcp" rateLimit.interval="...")

Notes
-----

.. include:: ratelimit-inline-note.rst

See also
--------
See also :doc:`../../configuration/modules/imptcp`.
