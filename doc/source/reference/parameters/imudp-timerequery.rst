.. _param-imudp-timerequery:
.. _imudp.parameter.module.timerequery:

TimeRequery
===========

.. index::
   single: imudp; TimeRequery
   single: TimeRequery

.. summary-start

Frequency of system time queries; lower values yield more precise timestamps.

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
This is a performance optimization. Getting the system time is very costly. With
this setting, imudp can be instructed to obtain the precise time only once every
n-times. This logic is only activated if messages come in at a very fast rate, so
doing less frequent time calls should usually be acceptable. The default value is
two, because we have seen that even without optimization the kernel often returns
twice the identical time. You can set this value as high as you like, but do so at
your own risk. The higher the value, the less precise the timestamp.

.. note::
   The time requery is based on executed system calls, not messages received.
   When batch sizes are used, multiple messages are obtained with one system
   call and all receive the same timestamp. At very high traffic the requery
   logic means time is queried only for every second batch by default. Do not
   set ``TimeRequery`` above 10 when input batches are used.

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
