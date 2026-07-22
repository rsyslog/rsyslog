.. _param-imbeats-maxcompressionratio:
.. _imbeats.parameter.input.maxcompressionratio:

maxCompressionRatio
===================

.. meta::
   :description: Maximum decompression ratio accepted for imbeats Lumberjack compressed frames.
   :keywords: rsyslog, imbeats, maxCompressionRatio, compression ratio, Lumberjack

.. index::
   single: imbeats; maxCompressionRatio
   single: maxCompressionRatio

.. summary-start

Limit how many output bytes a Lumberjack compressed frame may produce per input byte.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imbeats`.

:Name: maxCompressionRatio
:Scope: input
:Type: positive integer
:Default: input=256
:Required?: no
:Introduced: 8.2608.0

Description
-----------
Set the maximum permitted ratio of decompressed bytes to compressed input
bytes. The valid range is ``1`` to ``1000000``. A compressed frame is rejected
as soon as its decompressed output would exceed the compressed input size
multiplied by this value.

This ratio limit is enforced together with
:ref:`param-imbeats-maxdecompressedsize` and
:ref:`param-imbeats-maxinflightbytes`. A frame must satisfy all applicable
limits and is not acknowledged when any limit is exceeded.

Input usage
-----------
.. _param-imbeats-input-maxcompressionratio:
.. _imbeats.parameter.input.maxcompressionratio-usage:

RainerScript:

.. code-block:: rsyslog

   input(type="imbeats" port="5044" maxCompressionRatio="256")

YAML:

.. code-block:: yaml

   inputs:
     - type: imbeats
       port: 5044
       maxCompressionRatio: 256

See also
--------
See also :doc:`../../configuration/modules/imbeats`.
