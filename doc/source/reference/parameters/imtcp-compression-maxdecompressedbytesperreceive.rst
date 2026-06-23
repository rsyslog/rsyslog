.. _param-imtcp-compression-maxdecompressedbytesperreceive:
.. _imtcp.parameter.input.compression-maxdecompressedbytesperreceive:

.. meta::
   :description: Reference for the imtcp compression.maxDecompressedBytesPerReceive input parameter.
   :keywords: rsyslog, imtcp, compression.maxDecompressedBytesPerReceive, stream compression, decompression, security

compression.maxDecompressedBytesPerReceive
==========================================

.. index::
   single: imtcp; compression.maxDecompressedBytesPerReceive
   single: compression.maxDecompressedBytesPerReceive

.. summary-start

Limits decompressed bytes accepted from one compressed TCP receive operation.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imtcp`.

:Name: compression.maxDecompressedBytesPerReceive
:Scope: input/module
:Type: integer
:Default: input=67108864
:Required?: no
:Introduced: 8.2606.0

Description
-----------

``compression.maxDecompressedBytesPerReceive`` limits how many decompressed
bytes ``imtcp`` accepts from one compressed TCP receive operation when
``compression.mode="stream:always"`` is enabled. The default is 64 MiB.

If the limit is exceeded, ``imtcp`` logs the compressed stream as invalid and
closes the session. Already accepted data from earlier receive operations is
not affected. Set the value to ``0`` to disable this limit for tightly
controlled deployments that need a larger decompression burst.

Input usage
-----------
.. _param-imtcp-input-compression-maxdecompressedbytesperreceive:
.. _imtcp.parameter.input.compression-maxdecompressedbytesperreceive-usage:

.. code-block:: rsyslog

   input(
       type="imtcp"
       port="514"
       compression.mode="stream:always"
       compression.maxDecompressedBytesPerReceive="134217728"
   )

See also
--------

See also :ref:`param-imtcp-compression-mode`,
:ref:`param-imtcp-compression-maxexpansionratio`, and
:doc:`../../configuration/modules/imtcp`.
