.. _param-imptcp-flowcontrol:
.. _imptcp.parameter.input.flowcontrol:

flowControl
===========

.. index::
   single: imptcp; flowControl
   single: flowControl

.. summary-start

Throttles the sender when the receiver queue is nearly full.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imptcp`.

:Name: flowControl
:Scope: input
:Type: boolean
:Default: input=on
:Required?: no
:Introduced: at least 5.x, possibly earlier

Description
-----------
Flow control is used to throttle the sender if the receiver queue is
near-full preserving some space for input that can not be throttled.

Input usage
-----------
.. _param-imptcp-input-flowcontrol:
.. _imptcp.parameter.input.flowcontrol-usage:

.. code-block:: rsyslog

   input(type="imptcp" flowControl="...")

See also
--------
See also :doc:`../../configuration/modules/imptcp`.
