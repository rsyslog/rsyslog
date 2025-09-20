.. _param-mmexternal-interface-input:
.. _mmexternal.parameter.action.interface-input:

interface.input
===============

.. index::
   single: mmexternal; interface.input
   single: interface.input

.. summary-start

Selects which message representation mmexternal passes to the external plugin.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/mmexternal`.

:Name: interface.input
:Scope: action
:Type: string
:Default: msg
:Required?: no
:Introduced: 8.3.0

Description
-----------
This can either be "msg", "rawmsg" or "fulljson". In case of "fulljson", the message object is provided as a json object. Otherwise, the respective property is provided. This setting **must** match the external plugin's expectations. Check the external plugin documentation for what needs to be used.

Action usage
------------
.. _param-mmexternal-action-interface-input:
.. _mmexternal.parameter.action.interface-input-usage:

.. code-block:: rsyslog

   module(load="mmexternal")

   action(
       type="mmexternal"
       binary="/path/to/mmexternal.py"
       interface.input="fulljson"
   )

See also
--------
See also :doc:`../../configuration/modules/mmexternal`.
