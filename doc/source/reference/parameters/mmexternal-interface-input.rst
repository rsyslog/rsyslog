.. _param-mmexternal-interface-input:
.. _mmexternal.parameter.module.interface-input:

Interface.input
================

.. index::
   single: mmexternal; Interface.input
   single: Interface.input

.. summary-start

Selects which message representation mmexternal passes to the external plugin.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/mmexternal`.

:Name: Interface.input
:Scope: module
:Type: string
:Default: module=msg
:Required?: no
:Introduced: 8.3.0

Description
-----------
This can either be "msg", "rawmsg" or "fulljson". In case of "fulljson", the message object is provided as a json object. Otherwise, the respective property is provided. This setting **must** match the external plugin's expectations. Check the external plugin documentation for what needs to be used.

Module usage
------------
.. _param-mmexternal-module-interface-input:
.. _mmexternal.parameter.module.interface-input-usage:

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
