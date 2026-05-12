.. _param-imbeats-starvationprotection-maxreads:
.. _imbeats.parameter.input.starvationprotection-maxreads:

starvationProtection.maxReads
=============================

.. meta::
   :description: Maximum consecutive reads per imbeats session before another session can run.
   :keywords: rsyslog, imbeats, starvationProtection.maxReads, Beats sessions, fairness

.. index::
   single: imbeats; starvationProtection.maxReads
   single: starvationProtection.maxReads

.. summary-start

Limit consecutive reads from one imbeats session before other sessions can run.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imbeats`.

:Name: starvationProtection.maxReads
:Scope: input
:Type: non-negative integer
:Default: input=500
:Required?: no
:Introduced: 8.2606.0

Description
-----------
Set the maximum number of consecutive socket reads a worker performs for one
``imbeats`` session before yielding so other ready sessions can be processed.
This prevents one busy Beats sender from monopolizing the listener.

Use ``0`` to disable this fairness limit. Positive values cap the number of
consecutive reads per turn; smaller values favor fairness across sessions,
while larger values favor throughput for busy senders.

Input usage
-----------
.. _param-imbeats-input-starvationprotection-maxreads:
.. _imbeats.parameter.input.starvationprotection-maxreads-usage:

.. code-block:: rsyslog

   input(type="imbeats" port="5044" starvationProtection.maxReads="300")

See also
--------
See also :doc:`../../configuration/modules/imbeats`.
