.. _param-imtcp-streamdriver-mode:
.. _imtcp.parameter.module.streamdriver-mode:
.. _imtcp.parameter.input.streamdriver-mode:

StreamDriver.Mode
=================

.. index::
   single: imtcp; StreamDriver.Mode
   single: StreamDriver.Mode

.. summary-start

Sets the operational mode for the selected network stream driver.

.. summary-end

This parameter can be set at both the module and input levels.

:Name: StreamDriver.Mode
:Scope: module, input
:Type: integer
:Default: ``0`` (module), ``module parameter`` (input)
:Required?: no
:Introduced: at least 5.x, possibly earlier (module), 8.2106.0 (input)

Description
-----------
This parameter sets the mode for the currently selected :doc:`network stream driver <../../concepts/netstrm_drvr>`. The meaning of the mode number is specific to the selected driver. For example, for the ``gtls`` driver, a mode of ``1`` enables TLS-protected syslog.

This can be set at the module level to define a global default for all ``imtcp`` listeners. It can also be overridden on a per-input basis.

Module usage
------------
.. _imtcp.parameter.module.streamdriver-mode-usage:

.. code-block:: rsyslog

   module(load="imtcp" streamDriver.name="gtls" streamDriver.mode="1")

Input usage
-----------
.. _imtcp.parameter.input.streamdriver-mode-usage:

.. code-block:: rsyslog

   input(type="imtcp"
         port="514"
         streamDriver.name="gtls"
         streamDriver.mode="1")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.

.. _imtcp.parameter.legacy.inputtcpserverstreamdrivermode:
- $InputTCPServerStreamDriverMode — maps to StreamDriver.Mode (status: legacy)

.. index::
   single: imtcp; $InputTCPServerStreamDriverMode
   single: $InputTCPServerStreamDriverMode

See also
--------
See also :doc:`../../configuration/modules/imtcp`.
