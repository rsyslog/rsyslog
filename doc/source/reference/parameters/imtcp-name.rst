.. _param-imtcp-name:
.. _imtcp.parameter.input.name:

Name
====

.. index::
   single: imtcp; Name
   single: Name

.. summary-start

Assigns a custom name to the input listener for tagging and filtering.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imtcp`.

:Name: Name
:Scope: input
:Type: string
:Default: imtcp
:Required?: no
:Introduced: at least 5.x, possibly earlier

Description
-----------
This parameter sets a name for the ``inputname`` property, which is attached to all messages received through this listener. If no name is set, "imtcp" is used by default. Setting a name is not strictly necessary, but can be useful for applying filters based on the message's origin.

Input usage
-----------
.. _imtcp.parameter.input.name-usage:

.. code-block:: rsyslog

   input(type="imtcp" port="514" name="my-tcp-listener")

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
