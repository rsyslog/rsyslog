.. _param-imptcp-processonpoller:
.. _imptcp.parameter.module.processonpoller:

ProcessOnPoller
===============

.. index::
   single: imptcp; ProcessOnPoller
   single: ProcessOnPoller

.. summary-start

Processes messages on the poller thread when feasible to reduce resource use.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imptcp`.

:Name: ProcessOnPoller
:Scope: module
:Type: boolean
:Default: module=on
:Required?: no
:Introduced: at least 5.x, possibly earlier

Description
-----------
Instructs imptcp to process messages on poller thread opportunistically.
This leads to lower resource footprint (as poller thread doubles up as
message-processing thread too). "On" works best when imptcp is handling
low ingestion rates.

At high throughput though, it causes polling delay (as poller spends time
processing messages, which keeps connections in read-ready state longer
than they need to be, filling socket-buffer, hence eventually applying
backpressure).

It defaults to allowing messages to be processed on poller (for backward
compatibility).

Module usage
------------
.. _param-imptcp-module-processonpoller:
.. _imptcp.parameter.module.processonpoller-usage:

.. code-block:: rsyslog

   module(load="imptcp" processOnPoller="...")

See also
--------
See also :doc:`../../configuration/modules/imptcp`.
