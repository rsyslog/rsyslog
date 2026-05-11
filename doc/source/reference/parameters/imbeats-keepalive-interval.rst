.. _param-imbeats-keepalive-interval:
.. _imbeats.parameter.input.keepalive-interval:

KeepAlive.Interval
==================

.. meta::
   :description: TCP keepalive interval for imbeats sessions.
   :keywords: rsyslog, imbeats, keepalive interval

.. index::
   single: imbeats; KeepAlive.Interval
   single: KeepAlive.Interval

.. summary-start

Set the interval between TCP keepalive probes for imbeats sessions.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imbeats`.

:Name: KeepAlive.Interval
:Scope: input
:Type: integer
:Default: input=system default
:Required?: no
:Introduced: 8.2604.0

Description
-----------
Set the interval between TCP keepalive probes for imbeats sessions.

Input usage
-----------
.. _param-imbeats-input-keepalive-interval:
.. _imbeats.parameter.input.keepalive-interval-usage:

.. code-block:: rsyslog

   input(type="imbeats" port="5044" keepAlive.interval="10")

See also
--------
See also :doc:`../../configuration/modules/imbeats`.
