.. _param-imjournal-ratelimit-interval:
.. _imjournal.parameter.module.ratelimit-interval:

.. meta::
   :tag: module:imjournal
   :tag: parameter:Ratelimit.Interval

Ratelimit.Interval
==================

.. index::
   single: imjournal; Ratelimit.Interval
   single: Ratelimit.Interval

.. summary-start

Time window in seconds for rate limiting; 0 disables the limit.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imjournal`.

:Name: Ratelimit.Interval
:Scope: module
:Type: integer
:Default: module=600
:Required?: no
:Introduced: at least 5.x, possibly earlier

Description
-----------
Defines the interval over which message rate limiting is applied. If more than
``Ratelimit.Burst`` messages arrive during this interval, additional messages are
silently discarded and a discard notice is emitted at the end of the interval.

Module usage
------------
.. _param-imjournal-module-ratelimit-interval:
.. _imjournal.parameter.module.ratelimit-interval-usage:
.. code-block:: rsyslog

   module(load="imjournal" Ratelimit.Interval="...")

Notes
-----
- Setting the value to ``0`` disables rate limiting and is generally not recommended.

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
.. _imjournal.parameter.legacy.imjournalratelimitinterval:

- $imjournalRatelimitInterval — maps to Ratelimit.Interval (status: legacy)

.. index::
   single: imjournal; $imjournalRatelimitInterval
   single: $imjournalRatelimitInterval

See also
--------
See also :doc:`../../configuration/modules/imjournal`.
