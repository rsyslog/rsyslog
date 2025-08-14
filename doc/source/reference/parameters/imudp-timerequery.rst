.. _param-imudp-timerequery:
.. _imudp.parameter.module.timerequery:

TimeRequery
===========

.. index::
   single: imudp; TimeRequery
   single: TimeRequery

.. summary-start

Number of system calls before querying precise time again to optimize performance.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imudp`.

:Name: TimeRequery
:Scope: module
:Type: integer
:Default: module=2
:Required?: no
:Introduced: at least 5.x, possibly earlier

Description
-----------
Retrieving system time is costly. This parameter controls how often imudp obtains
the precise system time. A higher value reduces expensive time queries when
messages arrive very rapidly but lowers timestamp accuracy. Time is requested only
once per the specified number of system calls, not per message. When input batches
are used, all messages in a batch share the same timestamp. The default of 2
reflects that kernels often return the same time twice. Avoid values larger than
10 when batching.

Module usage
------------
.. _param-imudp-module-timerequery:
.. _imudp.parameter.module.timerequery-usage:
.. code-block:: rsyslog

   module(load="imudp" TimeRequery="...")

Notes
-----
- Setting very large values can cause significant timestamp deviations.

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.

.. _imudp.parameter.legacy.udpservertimerequery:
- $UDPServerTimeRequery — maps to TimeRequery (status: legacy)

.. index::
   single: imudp; $UDPServerTimeRequery
   single: $UDPServerTimeRequery

See also
--------
See also :doc:`../../configuration/modules/imudp`.

