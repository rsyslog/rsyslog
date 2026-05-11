.. _param-imbeats-keepalive:
.. _imbeats.parameter.input.keepalive:

KeepAlive
=========

.. meta::
   :description: Enable TCP keepalive for imbeats client sessions.
   :keywords: rsyslog, imbeats, keepalive

.. index::
   single: imbeats; KeepAlive
   single: KeepAlive

.. summary-start

Enable socket-level TCP keepalive on accepted imbeats connections.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imbeats`.

:Name: KeepAlive
:Scope: input
:Type: boolean
:Default: input=off
:Required?: no
:Introduced: 8.2604.0

Description
-----------
Enable socket-level TCP keepalive on accepted imbeats connections.

Input usage
-----------
.. _param-imbeats-input-keepalive:
.. _imbeats.parameter.input.keepalive-usage:

.. code-block:: rsyslog

   input(type="imbeats" port="5044" keepAlive="on")

See also
--------
See also :doc:`../../configuration/modules/imbeats`.
