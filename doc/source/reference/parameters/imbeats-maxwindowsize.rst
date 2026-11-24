.. SPDX-License-Identifier: Apache-2.0

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
:Default: input=2048
:Required?: no
:Introduced: 8.2604.0

Description
-----------
Set the maximum Lumberjack v2 window size accepted by this imbeats listener.
Connections that announce a larger window are rejected as protocol errors.

The window size a Beats client announces is the number of events in its
current batch, which can be as large as the client's ``bulk_max_size``. The
default of that setting is 2048 for the Logstash output, which is why the
default of this parameter is 2048. If you raise ``bulk_max_size`` on the
senders, set ``maxWindowSize`` to at least the same value; otherwise senders
with more queued events than ``maxWindowSize`` are rejected on every attempt
and never get an acknowledgement.

Input usage
-----------
.. _param-imbeats-input-maxwindowsize:
.. _imbeats.parameter.input.maxwindowsize-usage:

.. code-block:: rsyslog

   input(type="imbeats" port="5044" maxWindowSize="2048")

See also
--------
See also :doc:`../../configuration/modules/imbeats`.
