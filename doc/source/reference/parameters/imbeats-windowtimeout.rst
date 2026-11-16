.. SPDX-License-Identifier: Apache-2.0

.. _param-imbeats-windowtimeout:
.. _imbeats.parameter.input.windowtimeout:

windowTimeout
=============

.. meta::
   :description: Absolute deadline for completing an advertised imbeats Lumberjack window.
   :keywords: rsyslog, imbeats, windowTimeout, window deadline, Lumberjack

.. index::
   single: imbeats; windowTimeout
   single: windowTimeout

.. summary-start

Set an absolute deadline for completing an advertised Lumberjack event window.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imbeats`.

:Name: windowTimeout
:Scope: input
:Type: nonnegative integer
:Default: input=300
:Required?: no
:Introduced: 8.2608.0

Description
-----------
Set the maximum number of seconds allowed between accepting a Lumberjack
``W`` frame and receiving all events it advertises. The deadline is absolute:
receiving complete event frames does not extend it. This prevents a client from
retaining an incomplete batch and its session slot indefinitely by periodically
sending valid events.

When the deadline expires, ``imbeats`` closes the session, releases the
incomplete batch, and does not acknowledge it. The deadline ends before a
complete, sequence-valid batch enters submission. The value ``0`` disables the
window timeout. The independent :ref:`param-imbeats-idletimeout` and
:ref:`param-imbeats-frametimeout` settings may still close the session earlier.

Time a session spends waiting for a free
:ref:`worker thread <param-imbeats-workerthreads>` after the client has sent
data is local delay and is not counted against this deadline, so busy workers do not cause well-behaved clients to time out. This
applies where ``imbeats`` uses epoll (Linux); the portable poll fallback
cannot tell whether a queued session has pending data and counts that time.

Input usage
-----------
.. _param-imbeats-input-windowtimeout:
.. _imbeats.parameter.input.windowtimeout-usage:

RainerScript:

.. code-block:: rsyslog

   input(type="imbeats" port="5044" windowTimeout="300")

YAML:

.. code-block:: yaml

   inputs:
     - type: imbeats
       port: 5044
       windowTimeout: 300

See also
--------
See also :doc:`../../configuration/modules/imbeats`.
