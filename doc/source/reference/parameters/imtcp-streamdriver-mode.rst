.. _param-imtcp-streamdriver-mode:
.. _imtcp.parameter.module.streamdriver-mode:
.. _imtcp.parameter.input.streamdriver-mode:

StreamDriver.Mode
=================

.. index::
   single: imtcp; StreamDriver.Mode
   single: StreamDriver.Mode

.. summary-start

Sets the driver mode for the selected network stream driver.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imtcp`.

:Name: StreamDriver.Mode
:Scope: module, input
:Type: integer
:Default: module=0, input=module parameter
:Required?: no
:Introduced: at least 5.x, possibly earlier

Description
-----------
Sets the driver mode for the currently selected
:doc:`network stream driver <../../concepts/netstrm_drvr>`.
<number> is driver specific.
For the built-in TCP stream drivers, mode ``0`` is plain TCP and mode ``1`` is
TLS. Selecting a TLS-capable driver such as ``ossl``, ``gtls``, or ``mbedtls``
does not activate TLS unless the mode is ``1``. In
``global(compatibility.defaults.secure="strict")``, an omitted mode is promoted
to ``1`` when the effective stream driver is TLS-capable, while an explicit
mode ``0`` is rejected.

The same-named input parameter can override this module setting.


Module usage
------------
.. _param-imtcp-module-streamdriver-mode:
.. _imtcp.parameter.module.streamdriver-mode-usage:

.. code-block:: rsyslog

   module(load="imtcp" streamDriver.mode="...")

Input usage
-----------
.. _param-imtcp-input-streamdriver-mode:
.. _imtcp.parameter.input.streamdriver-mode-usage:

.. code-block:: rsyslog

   input(type="imtcp" port="514" streamDriver.mode="...")

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
