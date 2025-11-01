.. _param-omhttp-ratelimit-interval:
.. _omhttp.parameter.module.ratelimit-interval:

ratelimit.interval
==================

.. index::
   single: omhttp; ratelimit.interval
   single: ratelimit.interval

.. summary-start

Controls the duration, in seconds, of the rate-limiting window for the retry ruleset.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omhttp`.

:Name: ratelimit.interval
:Scope: module
:Type: integer
:Default: module=600
:Required?: no
:Introduced: Not specified

Description
-----------
This parameter sets the rate limiting behavior for the :ref:`param-omhttp-retry-ruleset`. It specifies the interval in seconds onto which rate-limiting is to be applied. If more than :ref:`param-omhttp-ratelimit-burst` messages are read during that interval, further messages up to the end of the interval are discarded. The number of messages discarded is emitted at the end of the interval (if there were any discards). Setting this to value zero turns off ratelimiting.

Module usage
------------
.. _omhttp.parameter.module.ratelimit-interval-usage:

.. code-block:: rsyslog

   module(load="omhttp")

   action(
       type="omhttp"
       retry="on"
       rateLimit.interval="300"
   )

See also
--------
See also :doc:`../../configuration/modules/omhttp`.
