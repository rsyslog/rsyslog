.. _param-omazuredce-compression:
.. _omazuredce.parameter.action.compression:

.. meta::
   :description: Reference for the omazuredce compression parameter.
   :keywords: rsyslog, omazuredce, compression, gzip, azure, logs ingestion

compression
===========

.. index::
   single: omazuredce; compression
   single: compression

.. summary-start

Selects whether ``omazuredce`` compresses the HTTP request body and which gzip
compression trade-off it uses.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omazuredce`.

:Name: compression
:Scope: action
:Type: word
:Default: off
:Required?: no
:Introduced: 8.2604.1

Description
-----------
Set ``compression="off"`` to send the JSON batch uncompressed.

Set ``compression`` to one of these values to gzip-compress the JSON request
body and send it with ``Content-Encoding: gzip``:

- ``speed``: use zlib's fastest compression level for lower CPU cost.
- ``default``: use zlib's default compression level to balance CPU usage and
  compression ratio.
- ``size``: use zlib's highest compression level to minimize transfer size at
  the cost of more CPU time.

``omazuredce`` still applies a ``max_batch_bytes`` size check
before sending the request, so enabling compression reduces network transfer
size without changing the module's batching semantics.

Action usage
------------
.. _omazuredce.parameter.action.compression-usage:

.. code-block:: rsyslog

   action(type="omazuredce" compression="default" ...)

YAML usage
----------

.. code-block:: yaml

   actions:
     - type: omazuredce
       compression: default

See also
--------
See also :doc:`../../configuration/modules/omazuredce`.
