.. _param-imbeats-keepalive-time:
.. _imbeats.parameter.input.keepalive-time:

KeepAlive.Time
==============

.. meta::
   :description: TCP keepalive idle time for imbeats sessions.
   :keywords: rsyslog, imbeats, keepalive time

.. index::
   single: imbeats; KeepAlive.Time
   single: KeepAlive.Time

.. summary-start

Set the idle time before TCP keepalive probing begins on imbeats sessions.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imbeats`.

:Name: KeepAlive.Time
:Scope: input
:Type: integer
:Default: input=system default
:Required?: no
:Introduced: 8.2604.0

Description
-----------
Set the idle time before TCP keepalive probing begins on imbeats sessions.

Input usage
-----------
.. _param-imbeats-input-keepalive-time:
.. _imbeats.parameter.input.keepalive-time-usage:

.. code-block:: rsyslog

   input(type="imbeats" port="5044" keepAlive.time="30")

See also
--------
See also :doc:`../../configuration/modules/imbeats`.
