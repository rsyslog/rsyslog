.. _param-imptcp-name:
.. _imptcp.parameter.input.name:

Name
====

.. index::
   single: imptcp; Name
   single: Name

.. summary-start

Sets the inputname property used for tagging messages and statistics.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imptcp`.

:Name: Name
:Scope: input
:Type: string
:Default: input=imptcp
:Required?: no
:Introduced: at least 5.x, possibly earlier

Description
-----------
Sets a name for the inputname property. If no name is set "imptcp" is
used by default. Setting a name is not strictly necessary, but can
be useful to apply filtering based on which input the message was
received from. Note that the name also shows up in
:doc:`impstats <../../configuration/modules/impstats>` logs.

Input usage
-----------
.. _param-imptcp-input-name:
.. _imptcp.parameter.input.name-usage:

.. code-block:: rsyslog

   input(type="imptcp" name="...")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.

.. _imptcp.parameter.legacy.inputptcpserverinputname:

- $InputPTCPServerInputName — maps to Name (status: legacy)

.. index::
   single: imptcp; $InputPTCPServerInputName
   single: $InputPTCPServerInputName

See also
--------
See also :doc:`../../configuration/modules/imptcp`.
