.. _param-omhttp-ratelimit-burst:
.. _omhttp.parameter.module.ratelimit-burst:

ratelimit.burst
===============

.. index::
   single: omhttp; ratelimit.burst
   single: ratelimit.burst

.. summary-start

Sets how many messages the retry ruleset may emit within each rate-limiting window.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omhttp`.

:Name: ratelimit.burst
:Scope: module
:Type: integer
:Default: module=20000
:Required?: no
:Introduced: Not specified

Description
-----------
This parameter sets the rate limiting behavior for the :ref:`param-omhttp-retry-ruleset`. It specifies the maximum number of messages that can be emitted within the :ref:`param-omhttp-ratelimit-interval` interval. For further information, see the description there.

Module usage
------------
.. _omhttp.parameter.module.ratelimit-burst-usage:

.. code-block:: rsyslog

   module(load="omhttp")

   action(
       type="omhttp"
       retry="on"
       rateLimit.burst="1000"
   )

See also
--------
See also :doc:`../../configuration/modules/omhttp`.
