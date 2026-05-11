.. _param-imbeats-streamdriver-prioritizesan:
.. _imbeats.parameter.input.streamdriver-prioritizesan:

StreamDriver.PrioritizeSAN
==========================

.. meta::
   :description: SAN preference for imbeats TLS name validation.
   :keywords: rsyslog, imbeats, prioritize SAN

.. index::
   single: imbeats; StreamDriver.PrioritizeSAN
   single: StreamDriver.PrioritizeSAN

.. summary-start

Prefer subject alternative names over common names when validating imbeats TLS peer names.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imbeats`.

:Name: StreamDriver.PrioritizeSAN
:Scope: input
:Type: boolean
:Default: input=off
:Required?: no
:Introduced: 8.2604.0

Description
-----------
Prefer subject alternative names over common names when validating imbeats TLS peer names.

Input usage
-----------
.. _param-imbeats-input-streamdriver-prioritizesan:
.. _imbeats.parameter.input.streamdriver-prioritizesan-usage:

.. code-block:: rsyslog

   input(type="imbeats" port="5044" streamDriver.prioritizeSAN="on")

See also
--------
See also :doc:`../../configuration/modules/imbeats`.
