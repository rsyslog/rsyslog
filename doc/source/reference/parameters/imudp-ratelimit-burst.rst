.. _param-imudp-ratelimit-burst:
.. _imudp.parameter.input.ratelimit-burst:

RateLimit.Burst
===============

.. index::
   single: imudp; RateLimit.Burst
   single: RateLimit.Burst

.. summary-start

Maximum messages permitted per rate-limiting burst.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imudp`.

:Name: RateLimit.Burst
:Scope: input
:Type: integer
:Default: input=10000
:Required?: no
:Introduced: 7.3.1

Description
-----------
Specifies the rate-limiting burst in number of messages.

Input usage
-----------
.. _param-imudp-input-ratelimit-burst:
.. _imudp.parameter.input.ratelimit-burst-usage:

.. code-block:: rsyslog

   input(type="imudp" RateLimit.Burst="...")

Notes
-----

.. include:: ratelimit-inline-note.rst

See also
--------
See also :doc:`../../configuration/modules/imudp`.
