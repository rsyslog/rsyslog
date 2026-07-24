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
