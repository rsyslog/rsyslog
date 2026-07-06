.. _param-imbeats-maxsessions:
.. _imbeats.parameter.input.maxsessions:

maxSessions
===========

.. meta::
   :description: Maximum simultaneous client sessions accepted by an imbeats input.
   :keywords: rsyslog, imbeats, maxSessions, Beats sessions, Lumberjack

.. index::
   single: imbeats; maxSessions
   single: maxSessions

.. summary-start

Limit how many simultaneous Beats client sessions an imbeats listener accepts.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imbeats`.

:Name: maxSessions
:Scope: input
:Type: positive integer
:Default: input=1000
:Required?: no
:Introduced: 8.2606.0

Description
-----------
Set the maximum number of simultaneous client sessions accepted by this
``imbeats`` listener. When the limit is reached, additional connection attempts
are rejected while existing sessions continue to run.

Use this limit to bound file descriptors, per-session memory, and sender fan-in
for a listener. Size it for the number of Beats or Elastic Agent senders that
may be connected at the same time, including reconnect overlap during restarts.
Values larger than ``UINT_MAX`` are accepted with a warning and capped at
``UINT_MAX``.

Input usage
-----------
.. _param-imbeats-input-maxsessions:
.. _imbeats.parameter.input.maxsessions-usage:

.. code-block:: rsyslog

   input(type="imbeats" port="5044" maxSessions="500")

See also
--------
See also :doc:`../../configuration/modules/imbeats`.
