.. _param-imudp-ratelimit-burst:
.. _imudp.parameter.module.ratelimit-burst:

RateLimit.Burst
===============

.. index::
   single: imudp; RateLimit.Burst
   single: RateLimit.Burst

.. summary-start

Defines the maximum number of messages accepted within a rate-limiting interval.

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
Specifies how many messages may arrive during a rate-limiting interval before additional ones are discarded.

Input usage
-----------
.. _param-imudp-input-ratelimit-burst:
.. _imudp.parameter.input.ratelimit-burst:
.. code-block:: rsyslog

   input(type="imudp" RateLimit.Burst="...")

Notes
-----
.. versionadded:: 7.3.1

See also
--------
See also :doc:`../../configuration/modules/imudp`.
