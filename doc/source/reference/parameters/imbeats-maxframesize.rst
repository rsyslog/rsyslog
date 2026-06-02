.. _param-imbeats-maxframesize:
.. _imbeats.parameter.input.maxframesize:

maxFrameSize
============

.. meta::
   :description: Maximum Lumberjack frame payload size accepted by imbeats.
   :keywords: rsyslog, imbeats, maxFrameSize, Lumberjack frame

.. index::
   single: imbeats; maxFrameSize
   single: maxFrameSize

.. summary-start

Limit the JSON or compressed frame payload size imbeats accepts from one Lumberjack frame.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imbeats`.

:Name: maxFrameSize
:Scope: input
:Type: positive integer
:Default: input=10485760
:Required?: no
:Introduced: 8.2604.0

Description
-----------
Set the maximum byte size accepted for an individual Lumberjack JSON frame or
compressed frame payload. Frames larger than this value are rejected before
payload allocation.

Input usage
-----------
.. _param-imbeats-input-maxframesize:
.. _imbeats.parameter.input.maxframesize-usage:

.. code-block:: rsyslog

   input(type="imbeats" port="5044" maxFrameSize="10485760")

See also
--------
See also :doc:`../../configuration/modules/imbeats`.
