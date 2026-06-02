.. _param-imbeats-workerthreads:
.. _imbeats.parameter.input.workerthreads:

workerThreads
=============

.. meta::
   :description: Number of worker threads used by an imbeats listener.
   :keywords: rsyslog, imbeats, workerThreads, Beats sessions, multiplexing

.. index::
   single: imbeats; workerThreads
   single: workerThreads

.. summary-start

Set how many worker threads an imbeats listener uses to process client sessions.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imbeats`.

:Name: workerThreads
:Scope: input
:Type: positive integer
:Default: input=2
:Required?: no
:Introduced: 8.2606.0

Description
-----------
Set the number of worker threads used by this ``imbeats`` listener. A value of
``1`` keeps processing single-threaded. Higher values allow multiple client
sessions to be processed in parallel when the platform and stream layer support
session multiplexing.

Increasing this value can improve throughput for many active senders, but each
worker consumes system resources. Avoid setting it higher than the expected
active sender count or available CPU capacity.

Input usage
-----------
.. _param-imbeats-input-workerthreads:
.. _imbeats.parameter.input.workerthreads-usage:

.. code-block:: rsyslog

   input(type="imbeats" port="5044" workerThreads="4")

See also
--------
See also :doc:`../../configuration/modules/imbeats`.
