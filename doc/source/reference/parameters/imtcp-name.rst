.. _param-imtcp-name:
.. _imtcp.parameter.input.name:

Name
====

.. index::
   single: imtcp; Name
   single: Name

.. summary-start

Sets the value for the ``inputname`` property.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imtcp`.

:Name: Name
:Scope: input
:Type: string (see :doc:`../../rainerscript/constant_strings`)
:Default: input=imtcp
:Required?: no
:Introduced: at least 5.x, possibly earlier

Description
-----------
Sets a name for the ``inputname`` property. If no name is set, ``imtcp`` is used by default.
Setting a name is not strictly necessary but can be useful to filter based on which input
received the message.

Input usage
-----------
.. _param-imtcp-input-name:
.. _imtcp.parameter.input.name-usage:

.. code-block:: rsyslog

   input(type="imtcp" name="tcp1" port="514")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.

.. _imtcp.parameter.legacy.inputtcpserverinputname:

- $InputTCPServerInputName — maps to Name (status: legacy)

.. index::
   single: imtcp; $InputTCPServerInputName
   single: $InputTCPServerInputName

See also
--------
See also :doc:`../../configuration/modules/imtcp`.
