.. _param-imptcp-ratelimit-burst:
.. _imptcp.parameter.input.ratelimit-burst:

RateLimit.Burst
===============

.. index::
   single: imptcp; RateLimit.Burst
   single: RateLimit.Burst

.. summary-start

Sets the rate-limiting burst size in number of messages.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imptcp`.

:Name: RateLimit.Burst
:Scope: input
:Type: integer
:Default: input=10000
:Required?: no
:Introduced: at least 5.x, possibly earlier

Description
-----------
Specifies the rate-limiting burst in number of messages.

Input usage
-----------
.. _param-imptcp-input-ratelimit-burst:
.. _imptcp.parameter.input.ratelimit-burst-usage:

.. code-block:: rsyslog

   input(type="imptcp" rateLimit.burst="...")

See also
--------
See also :doc:`../../configuration/modules/imptcp`.
