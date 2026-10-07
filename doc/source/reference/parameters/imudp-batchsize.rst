.. meta::
   :description: Configure the maximum imudp recvmmsg receive batch size.
   :keywords: rsyslog, imudp, BatchSize, recvmmsg, UDP

.. _param-imudp-batchsize:
.. _imudp.parameter.module.batchsize:

BatchSize
=========

.. index::
   single: imudp; BatchSize
   single: BatchSize

.. summary-start

Maximum messages retrieved per ``recvmmsg()`` call when available.

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
This parameter is only meaningful if the system supports ``recvmmsg()`` (newer
Linux systems do this). The parameter is silently ignored if the system does not
support it. If supported, it must be between ``1`` and ``INT_MAX`` and sets the
maximum number of UDP messages that can be obtained with a single OS call.

For systems with high UDP traffic, a relatively high batch size can reduce
system overhead and improve performance. However, this parameter should not be
overdone. For each batch element, rsyslog statically allocates the configured
maximum message size plus receive metadata. Configurations whose combined
dimensions cannot be represented by the platform's address space are rejected.
A too-high number can also reduce efficiency because the metadata structures
must be initialized before each OS call. We suggest not setting it above
``128`` unless measurements show a benefit.

Module usage
------------
.. _param-imudp-module-batchsize:
.. _imudp.parameter.module.batchsize-usage:

.. code-block:: rsyslog

   module(load="imudp" BatchSize="...")

See also
--------
See also :doc:`../../configuration/modules/imudp`.
