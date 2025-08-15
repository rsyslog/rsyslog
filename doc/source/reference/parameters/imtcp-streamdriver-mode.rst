.. _param-imtcp-streamdriver-mode:
.. _imtcp.parameter.module.streamdriver-mode:

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
:Scope: module
:Type: integer
:Default: module=0
:Required?: no
:Introduced: at least 5.x, possibly earlier

Description
-----------
Sets the driver mode for the currently selected
:doc:`network stream driver <../../concepts/netstrm_drvr>`.
<number> is driver specific.

The same-named input parameter can override this module setting.


Module usage
------------
.. _param-imtcp-module-streamdriver-mode:
.. _imtcp.parameter.module.streamdriver-mode-usage:

.. code-block:: rsyslog

   module(load="imtcp" streamDriver.mode="...")

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

