.. _param-imudp-timerequery:
.. _imudp.parameter.module.timerequery:

TimeRequery
===========

.. index::
   single: imudp; TimeRequery
   single: TimeRequery

.. summary-start

Controls how often `imudp` retrieves the precise system time to reduce overhead.

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
Obtains the current time only after the specified number of system calls to optimize performance when messages arrive rapidly.

Module usage
------------
.. _param-imudp-module-timerequery:
.. _imudp.parameter.module.timerequery-usage:
.. code-block:: rsyslog

   module(load="imudp" TimeRequery="...")

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
