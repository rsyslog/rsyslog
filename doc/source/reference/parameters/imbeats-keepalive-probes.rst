.. _param-imbeats-keepalive-probes:
.. _imbeats.parameter.input.keepalive-probes:

KeepAlive.Probes
================

.. meta::
   :description: TCP keepalive probe count for imbeats sessions.
   :keywords: rsyslog, imbeats, keepalive probes

.. index::
   single: imbeats; KeepAlive.Probes
   single: KeepAlive.Probes

.. summary-start

Set how many TCP keepalive probes are sent before the peer is considered dead.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imbeats`.

:Name: KeepAlive.Probes
:Scope: input
:Type: integer
:Default: input=system default
:Required?: no
:Introduced: 8.2604.0

Description
-----------
Set how many TCP keepalive probes are sent before the peer is considered dead.

Input usage
-----------
.. _param-imbeats-input-keepalive-probes:
.. _imbeats.parameter.input.keepalive-probes-usage:

.. code-block:: rsyslog

   input(type="imbeats" port="5044" keepAlive.probes="5")

See also
--------
See also :doc:`../../configuration/modules/imbeats`.
