.. _param-imbeats-maxwindowsize:
.. _imbeats.parameter.input.maxwindowsize:

maxWindowSize
=============

.. meta::
   :description: Maximum Lumberjack window size accepted by imbeats.
   :keywords: rsyslog, imbeats, maxWindowSize, Lumberjack window

.. index::
   single: imbeats; maxWindowSize
   single: maxWindowSize

.. summary-start

Limit how many events imbeats accepts in one Lumberjack batch window.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imbeats`.

:Name: maxWindowSize
:Scope: input
:Type: positive integer
:Default: input=1024
:Required?: no
:Introduced: 8.2604.0

Description
-----------
Set the maximum Lumberjack v2 window size accepted by this imbeats listener.
Connections that announce a larger window are rejected as protocol errors.

Input usage
-----------
.. _param-imbeats-input-maxwindowsize:
.. _imbeats.parameter.input.maxwindowsize-usage:

.. code-block:: rsyslog

   input(type="imbeats" port="5044" maxWindowSize="1024")

See also
--------
See also :doc:`../../configuration/modules/imbeats`.
