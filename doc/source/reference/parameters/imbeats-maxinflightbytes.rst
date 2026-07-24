.. _param-imbeats-maxinflightbytes:
.. _imbeats.parameter.input.maxinflightbytes:

maxInFlightBytes
================

.. meta::
   :description: Maximum memory charged to in-flight Lumberjack input state for one imbeats instance.
   :keywords: rsyslog, imbeats, maxInFlightBytes, memory limit, Lumberjack

.. index::
   single: imbeats; maxInFlightBytes
   single: maxInFlightBytes

.. summary-start

Limit the memory charged to incomplete Lumberjack input across an imbeats instance.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imbeats`.

:Name: maxInFlightBytes
:Scope: input
:Type: positive integer
:Default: input=268435456
:Required?: no
:Introduced: 8.2608.0

Description
-----------
Set the maximum number of bytes that one ``imbeats`` input instance may charge
to incomplete receive buffers, decompression scratch space, retained event
copies, and lazily allocated window descriptors. The valid range is ``1`` to
``2000000000`` bytes.

Accounting is exact across all sessions of the input instance. A session that
would exceed the limit is closed, its charged resources are released, and its
incomplete batch is not acknowledged. Increase this value only when legitimate
concurrent batches require more aggregate memory.

Input usage
-----------
.. _param-imbeats-input-maxinflightbytes:
.. _imbeats.parameter.input.maxinflightbytes-usage:

RainerScript:

.. code-block:: rsyslog

   input(type="imbeats" port="5044" maxInFlightBytes="268435456")

YAML:

.. code-block:: yaml

   inputs:
     - type: imbeats
       port: 5044
       maxInFlightBytes: 268435456

See also
--------
See also :doc:`../../configuration/modules/imbeats`.
