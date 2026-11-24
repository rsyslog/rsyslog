.. SPDX-License-Identifier: Apache-2.0

.. _param-imbeats-frametimeout:
.. _imbeats.parameter.input.frametimeout:

frameTimeout
============

.. meta::
   :description: Absolute deadline for completing an in-progress imbeats Lumberjack frame read.
   :keywords: rsyslog, imbeats, frameTimeout, frame deadline, Lumberjack

.. index::
   single: imbeats; frameTimeout
   single: frameTimeout

.. summary-start

Set an absolute deadline for completing an in-progress Lumberjack frame read.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imbeats`.

:Name: frameTimeout
:Scope: input
:Type: nonnegative integer
:Default: input=30
:Required?: no
:Introduced: 8.2608.0

Description
-----------
Set the maximum number of seconds allowed to complete an in-progress
Lumberjack frame read. The deadline is absolute: receiving additional bytes
does not extend it. This prevents a client from retaining frame state
indefinitely by sending a slow trickle of data.

When the deadline expires, ``imbeats`` closes the session and does not
acknowledge the incomplete batch. The value ``0`` disables the frame timeout.
The independent :ref:`param-imbeats-idletimeout` may still close a session that
makes no byte progress.

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
.. _param-imbeats-input-frametimeout:
.. _imbeats.parameter.input.frametimeout-usage:

RainerScript:

.. code-block:: rsyslog

   input(type="imbeats" port="5044" frameTimeout="30")

YAML:

.. code-block:: yaml

   inputs:
     - type: imbeats
       port: 5044
       frameTimeout: 30

See also
--------
See also :doc:`../../configuration/modules/imbeats`.
