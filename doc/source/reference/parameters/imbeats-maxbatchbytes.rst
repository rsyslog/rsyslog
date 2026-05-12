.. _param-imbeats-maxbatchbytes:
.. _imbeats.parameter.input.maxbatchbytes:

maxBatchBytes
=============

.. meta::
   :description: Maximum cumulative Lumberjack batch payload size accepted by imbeats.
   :keywords: rsyslog, imbeats, maxBatchBytes, Lumberjack batch, size limit

.. index::
   single: imbeats; maxBatchBytes
   single: maxBatchBytes

.. summary-start

Limit the cumulative JSON payload bytes accepted in one Lumberjack batch.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imbeats`.

:Name: maxBatchBytes
:Scope: input
:Type: positive integer
:Default: input=67108864
:Required?: no
:Introduced: 8.2604.0

Description
-----------
Set the maximum cumulative byte size of JSON event payloads accepted in one
Lumberjack batch window. Batches that exceed this limit are rejected before all
payloads are retained in memory.

Input usage
-----------
.. _param-imbeats-input-maxbatchbytes:
.. _imbeats.parameter.input.maxbatchbytes-usage:

.. code-block:: rsyslog

   input(type="imbeats" port="5044" maxBatchBytes="67108864")

See also
--------
See also :doc:`../../configuration/modules/imbeats`.
