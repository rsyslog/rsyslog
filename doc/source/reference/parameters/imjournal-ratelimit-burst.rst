.. _param-imjournal-ratelimit-burst:
.. _imjournal.parameter.module.ratelimit-burst:

.. meta::
   :tag: module:imjournal
   :tag: parameter:Ratelimit.Burst

Ratelimit.Burst
===============

.. index::
   single: imjournal; Ratelimit.Burst
   single: Ratelimit.Burst

.. summary-start

Maximum messages accepted within each rate-limit window.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imjournal`.

:Name: Ratelimit.Burst
:Scope: module
:Type: integer
:Default: module=20000
:Required?: no
:Introduced: at least 5.x, possibly earlier

Description
-----------
Specifies how many messages are permitted during each ``Ratelimit.Interval``
period. Messages beyond this threshold are discarded until the interval expires.

Module usage
------------
.. _param-imjournal-module-ratelimit-burst:
.. _imjournal.parameter.module.ratelimit-burst-usage:
.. code-block:: rsyslog

   module(load="imjournal" Ratelimit.Burst="...")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
.. _imjournal.parameter.legacy.imjournalratelimitburst:

- $imjournalRatelimitBurst — maps to Ratelimit.Burst (status: legacy)

.. index::
   single: imjournal; $imjournalRatelimitBurst
   single: $imjournalRatelimitBurst

See also
--------
See also :doc:`../../configuration/modules/imjournal`.
