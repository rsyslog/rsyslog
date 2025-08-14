.. _param-imudp-batchsize:
.. _imudp.parameter.module.batchsize:

BatchSize
=========

.. index::
   single: imudp; BatchSize
   single: BatchSize

.. summary-start

Limits the number of UDP messages `imudp` reads per system call when `recvmmsg()` is available.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imudp`.

:Name: BatchSize
:Scope: module
:Type: integer
:Default: module=32
:Required?: no
:Introduced: at least 8.x, possibly earlier

Description
-----------
Controls the maximum count of packets collected in one OS call to balance throughput against memory use.

Module usage
------------
.. _param-imudp-module-batchsize:
.. _imudp.parameter.module.batchsize-usage:
.. code-block:: rsyslog

   module(load="imudp" BatchSize="...")

See also
--------
See also :doc:`../../configuration/modules/imudp`.
