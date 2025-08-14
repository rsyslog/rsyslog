.. _param-imtcp-maxlisteners:
.. _imtcp.parameter.module.maxlisteners:
.. _imtcp.parameter.input.maxlisteners:

MaxListeners
============

.. index::
   single: imtcp; MaxListeners
   single: MaxListeners

.. summary-start

Sets the maximum number of concurrent TCP listeners this module can run.

.. summary-end

This parameter can be set at both the module and input levels.

:Name: MaxListeners
:Scope: module, input
:Type: integer
:Default: ``20`` (module), ``module parameter`` (input)
:Required?: no
:Introduced: at least 5.x, possibly earlier (module), 8.2106.0 (input)

Description
-----------
This parameter defines the maximum number of TCP server ports (listeners) that the ``imtcp`` module can support simultaneously.

This can be set at the module level to define a global default for all ``imtcp`` listeners. This value must be set when the module is loaded, before any ``input()`` listeners are defined. It can also be overridden on a per-input basis.

Module usage
------------
.. _imtcp.parameter.module.maxlisteners-usage:

.. code-block:: rsyslog

   module(load="imtcp" maxListeners="10")

Input usage
-----------
.. _imtcp.parameter.input.maxlisteners-usage:

.. code-block:: rsyslog

   input(type="imtcp" port="514" maxListeners="5")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.

.. _imtcp.parameter.legacy.inputtcpmaxlisteners:
- $InputTCPMaxListeners — maps to MaxListeners (status: legacy)

.. index::
   single: imtcp; $InputTCPMaxListeners
   single: $InputTCPMaxListeners

See also
--------
See also :doc:`../../configuration/modules/imtcp`.
