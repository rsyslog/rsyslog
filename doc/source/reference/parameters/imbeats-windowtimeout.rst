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
:ref:`worker thread <param-imbeats-workerthreads>` is local delay and is not
counted against this deadline in two cases: when the session became ready
because the peer acted (it sent data, closed, or made room for an
acknowledgement), and when the session was requeued by
:ref:`starvation protection <param-imbeats-starvationprotection-maxreads>`
after it had already made progress. Busy workers therefore do not turn a
well-behaved client into a timeout.

This forgiveness applies where ``imbeats`` uses epoll (Linux). The portable
poll fallback queues sessions without knowing whether input is available, so
its accept and re-arm hand-offs are not forgiven there and an idle peer still
times out.

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
