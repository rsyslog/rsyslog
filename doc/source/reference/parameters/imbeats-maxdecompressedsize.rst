.. _param-imbeats-maxdecompressedsize:
.. _imbeats.parameter.input.maxdecompressedsize:

maxDecompressedSize
===================

.. meta::
   :description: Maximum decompressed Lumberjack payload size accepted by imbeats.
   :keywords: rsyslog, imbeats, maxDecompressedSize, Lumberjack compression

.. index::
   single: imbeats; maxDecompressedSize
   single: maxDecompressedSize

.. summary-start

Limit how large a compressed Lumberjack frame may become after decompression.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imbeats`.

:Name: maxDecompressedSize
:Scope: input
:Type: positive integer
:Default: input=67108864
:Required?: no
:Introduced: 8.2604.0

Description
-----------
Set the maximum byte size accepted after decompressing a Lumberjack compressed
frame. Frames that expand beyond this value are rejected before unbounded memory
growth can occur.

Input usage
-----------
.. _param-imbeats-input-maxdecompressedsize:
.. _imbeats.parameter.input.maxdecompressedsize-usage:

.. code-block:: rsyslog

   input(type="imbeats" port="5044" maxDecompressedSize="67108864")

See also
--------
See also :doc:`../../configuration/modules/imbeats`.
