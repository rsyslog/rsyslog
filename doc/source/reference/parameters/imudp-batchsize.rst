.. _param-imudp-batchsize:
.. _imudp.parameter.module.batchsize:

BatchSize
=========

.. index::
   single: imudp; BatchSize
   single: BatchSize

.. summary-start

Maximum number of UDP messages read with one ``recvmmsg()`` call when batching is supported.

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
If the platform supports ``recvmmsg()``, this sets how many UDP messages can be
obtained with a single OS call. Larger batches reduce system overhead for high
traffic but require preallocated buffers for each potential message. Too-high
values lead to inefficiency because structures must be initialized before the
call. Values above 128 are generally not recommended.

Module usage
------------
.. _param-imudp-module-batchsize:
.. _imudp.parameter.module.batchsize-usage:
.. code-block:: rsyslog

   module(load="imudp" BatchSize="...")

Notes
-----
- Ignored silently if ``recvmmsg()`` is not supported by the system.

See also
--------
See also :doc:`../../configuration/modules/imudp`.

