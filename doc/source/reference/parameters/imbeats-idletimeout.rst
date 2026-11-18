.. SPDX-License-Identifier: Apache-2.0

.. _param-imbeats-idletimeout:
.. _imbeats.parameter.input.idletimeout:

idleTimeout
===========

.. meta::
   :description: Close imbeats sessions that make no byte progress for a configured interval.
   :keywords: rsyslog, imbeats, idleTimeout, session timeout, Lumberjack

.. index::
   single: imbeats; idleTimeout
   single: idleTimeout

.. summary-start

Close an imbeats session when it makes no byte progress for the configured time.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imbeats`.

:Name: idleTimeout
:Scope: input
:Type: nonnegative integer
:Default: input=60
:Required?: no
:Introduced: 8.2608.0

Description
-----------
Set the maximum number of seconds that a client session may remain connected
without receiving any new bytes. When the timeout expires, ``imbeats`` closes
the session and does not acknowledge any incomplete batch. The value ``0``
disables the idle timeout.

This progress-based timeout applies even when the session has not started a
Lumberjack frame. It pauses while a complete, validated batch is submitted to
rsyslog and resumes when ``imbeats`` prepares the acknowledgement, so slow
local processing cannot turn a successfully submitted batch into an
unacknowledged retransmission. The resumed interval also bounds a client that
stops reading its acknowledgement. Use :ref:`param-imbeats-frametimeout` to
place an absolute deadline on a partially received frame.

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
.. _param-imbeats-input-idletimeout:
.. _imbeats.parameter.input.idletimeout-usage:

RainerScript:

.. code-block:: rsyslog

   input(type="imbeats" port="5044" idleTimeout="60")

YAML:

.. code-block:: yaml

   inputs:
     - type: imbeats
       port: 5044
       idleTimeout: 60

See also
--------
See also :doc:`../../configuration/modules/imbeats`.
