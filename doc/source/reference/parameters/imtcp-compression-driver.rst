.. _param-imtcp-compression-driver:
.. _imtcp.parameter.input.compression-driver:

.. meta::
   :description: Reference for the imtcp compression.driver input parameter.
   :keywords: rsyslog, imtcp, compression.driver, stream compression, zlib, zstd

compression.driver
==================

.. index::
   single: imtcp; compression.driver
   single: compression.driver

.. summary-start

Selects the decompression driver for ``imtcp`` stream-compressed TCP input.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imtcp`.

:Name: compression.driver
:Scope: input/module
:Type: word
:Default: input=zlib
:Required?: no
:Introduced: 8.2606.0

Description
-----------

``compression.driver`` selects the decompression library used when
``compression.mode="stream:always"`` is configured.

Accepted values are:

- ``zlib``: default stream decompression driver.
- ``zstd``: zstd stream decompression driver. This requires rsyslog to be built
  with libzstd support, and the ``omfwd`` sender must also use
  ``compression.driver="zstd"``.

The driver setting must match the sender. A zstd-compressed stream cannot be
received with ``compression.driver="zlib"``, and a zlib-compressed stream
cannot be received with ``compression.driver="zstd"``.

Input usage
-----------
.. _param-imtcp-input-compression-driver:
.. _imtcp.parameter.input.compression-driver-usage:

.. code-block:: rsyslog

   input(
       type="imtcp"
       port="514"
       compression.mode="stream:always"
       compression.driver="zstd"
   )

See also
--------

See also :ref:`param-imtcp-compression-mode` and
:doc:`../../configuration/modules/imtcp`.
