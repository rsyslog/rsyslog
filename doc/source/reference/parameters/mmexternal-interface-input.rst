.. _param-mmexternal-interface-input:
.. _mmexternal.parameter.input.interface-input:

interface.input
===============

.. index::
   single: mmexternal; interface.input
   single: interface.input

.. summary-start

Selects which message representation mmexternal passes to the external
plugin.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/mmexternal`.

:Name: interface.input
:Scope: input
:Type: string
:Default: msg
:Required?: no
:Introduced: 8.3.0

Description
-----------
This parameter controls what part of the message is passed to the external
program's standard input. It can be set to one of the following values:

* ``"msg"`` (default): Passes the MSG part of the syslog message.
* ``"rawmsg"``: Passes the complete, original syslog message as received by
  rsyslog, including headers.
* ``"fulljson"``: Passes the rsyslog message object represented as a JSON
  object.

This setting **must** match the external plugin's expectations. Check the
external plugin documentation for what needs to be used.

.. note::
   When processing multi-line messages, you **must** use ``"fulljson"``. This
   ensures that embedded line breaks are encoded properly and do not cause
   parsing errors in the external plugin interface.

Input usage
-----------
.. _mmexternal.parameter.input.interface-input-usage:

.. code-block:: rsyslog

   module(load="mmexternal")

   action(
       type="mmexternal"
       binary="/path/to/mmexternal.py"
       interfaceInput="fulljson"
   )

See also
--------
See also :doc:`../../configuration/modules/mmexternal`.
