.. _param-imudp-ratelimit-burst:
.. _imudp.parameter.module.ratelimit-burst:

RateLimit.Burst
===============

.. index::
   single: imudp; RateLimit.Burst
   single: RateLimit.Burst

.. summary-start

Maximum number of messages permitted within one rate-limiting interval.

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
Sets the number of messages allowed to pass within the current rate-limiting
interval before excess messages are discarded or delayed.

Input usage
-----------
.. _param-imudp-input-ratelimit-burst:
.. _imudp.parameter.input.ratelimit-burst:
.. code-block:: rsyslog

   input(type="imudp" RateLimit.Burst="...")

See also
--------
See also :doc:`../../configuration/modules/imudp`.

