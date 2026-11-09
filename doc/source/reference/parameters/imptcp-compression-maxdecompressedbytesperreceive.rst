.. SPDX-License-Identifier: Apache-2.0

.. _param-imptcp-compression-maxdecompressedbytesperreceive:
.. _imptcp.parameter.input.compression-maxdecompressedbytesperreceive:

.. meta::
   :description: Reference for the imptcp compression.maxDecompressedBytesPerReceive input parameter.
   :keywords: rsyslog, imptcp, compression.maxDecompressedBytesPerReceive, stream compression, decompression

compression.maxDecompressedBytesPerReceive
==========================================

.. index::
   single: imptcp; compression.maxDecompressedBytesPerReceive
   single: compression.maxDecompressedBytesPerReceive

.. summary-start

Limits decompressed bytes accepted from one compressed TCP receive operation.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imptcp`.

:Name: compression.maxDecompressedBytesPerReceive
:Scope: input
:Type: integer
:Default: input=67108864
:Required?: no
:Introduced: 8.2612.0

Description
-----------

``compression.maxDecompressedBytesPerReceive`` limits how many decompressed
bytes ``imptcp`` accepts from one compressed TCP receive operation when
``compression.mode`` selects stream decompression. The default is 64 MiB.

If the limit is exceeded, ``imptcp`` logs the compressed stream as invalid,
discards the remaining stream data, and closes the session. Data accepted from
earlier receive operations is not affected. Set the value to ``0`` to disable
this limit for tightly controlled deployments that need a larger decompression
burst.

The parameter behaves like the identically named :doc:`imtcp
<../../configuration/modules/imtcp>` parameter.

Input usage
-----------
.. _param-imptcp-input-compression-maxdecompressedbytesperreceive:
.. _imptcp.parameter.input.compression-maxdecompressedbytesperreceive-usage:

.. code-block:: rsyslog

   input(
       type="imptcp"
       port="514"
       compression.mode="stream:always"
       compression.maxDecompressedBytesPerReceive="134217728"
   )

See also
--------

See also :ref:`param-imptcp-compression-mode`,
:ref:`param-imptcp-compression-maxexpansionratio`, and
:doc:`../../configuration/modules/imptcp`.
