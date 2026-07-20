.. _param-imtcp-compression-maxtotalzstdwindowbytes:
.. _imtcp.parameter.input.compression-maxtotalzstdwindowbytes:

.. meta::
   :description: Reference for the imtcp compression.maxTotalZstdWindowBytes parameter.
   :keywords: rsyslog, imtcp, compression.maxTotalZstdWindowBytes, zstd, decoder window, memory limit

compression.maxTotalZstdWindowBytes
===================================

.. index::
   single: imtcp; compression.maxTotalZstdWindowBytes
   single: compression.maxTotalZstdWindowBytes

.. summary-start

Limits aggregate advertised zstd decoder-window bytes across active sessions
for one ``imtcp`` listener.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imtcp`.

:Name: compression.maxTotalZstdWindowBytes
:Scope: input/module
:Type: integer
:Default: 0
:Required?: no
:Introduced: 8.2608.0

Description
-----------

``compression.maxTotalZstdWindowBytes`` limits the sum of advertised zstd
decoder-window sizes retained by active compressed sessions on one ``imtcp``
listener. It applies only when ``compression.mode="stream:always"`` and
``compression.driver="zstd"`` are active. A frame whose reservation would
exceed the limit is rejected and its session is closed without affecting
already admitted sessions.

The default ``0`` leaves aggregate decoder-window memory unlimited for
compatibility. This setting is independent of
:ref:`param-imtcp-compression-maxdecompressedbytesperreceive`, which limits
decompressed output from one TCP receive operation.

The secure-default policy controls omitted values:

- ``compatibility.defaults.secure="backward-compatible"`` accepts the
  unlimited default silently.
- ``compatibility.defaults.secure="warn"`` accepts it and emits a startup
  warning. Explicitly setting ``0`` acknowledges unlimited operation and
  suppresses that warning.
- ``compatibility.defaults.secure="strict"`` requires an explicitly configured
  positive value and rejects omitted or explicit-zero values.

The accounting uses the window size declared by each zstd frame. It bounds the
dominant retained decoder-window allocation, but does not attempt to account
for every byte of decoder bookkeeping or unrelated per-connection memory.

RainerScript usage
------------------

.. code-block:: rsyslog

   input(
       type="imtcp"
       address="127.0.0.1"
       port="514"
       compression.mode="stream:always"
       compression.driver="zstd"
       compression.maxTotalZstdWindowBytes="536870912"
   )

YAML usage
----------

.. code-block:: yaml

   inputs:
     - type: imtcp
       address: "127.0.0.1"
       port: "514"
       compression.mode: "stream:always"
       compression.driver: "zstd"
       compression.maxTotalZstdWindowBytes: "536870912"

The same parameter may be placed on ``module(load="imtcp" ...)`` or its YAML
module entry to provide an explicitly configured default inherited by inputs.

See also
--------

See also :ref:`param-imtcp-compression-mode`,
:ref:`param-imtcp-compression-driver`,
:ref:`param-imtcp-compression-maxdecompressedbytesperreceive`, and
:doc:`../../configuration/modules/imtcp`.
