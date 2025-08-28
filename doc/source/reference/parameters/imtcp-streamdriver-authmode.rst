.. _param-imtcp-streamdriver-authmode:
.. _imtcp.parameter.module.streamdriver-authmode:
.. _imtcp.parameter.input.streamdriver-authmode:

StreamDriver.AuthMode
=====================

.. index::
   single: imtcp; StreamDriver.AuthMode
   single: StreamDriver.AuthMode

.. summary-start

Sets stream driver authentication mode.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imtcp`.

:Name: StreamDriver.AuthMode
:Scope: module, input
:Type: string (see :doc:`../../rainerscript/constant_strings`)
:Default: module=none, input=module parameter
:Required?: no
:Introduced: at least 5.x, possibly earlier

Description
-----------
Sets stream driver authentication mode. Possible values and meaning
depend on the :doc:`network stream driver <../../concepts/netstrm_drvr>`.
used.

The same-named input parameter can override this module setting.


Module usage
------------
.. _param-imtcp-module-streamdriver-authmode:
.. _imtcp.parameter.module.streamdriver-authmode-usage:

.. code-block:: rsyslog

   module(load="imtcp" streamDriver.authMode="...")

Input usage
-----------
.. _param-imtcp-input-streamdriver-authmode:
.. _imtcp.parameter.input.streamdriver-authmode-usage:

.. code-block:: rsyslog

   input(type="imtcp" port="514" streamDriver.authMode="...")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.

.. _imtcp.parameter.legacy.inputtcpserverstreamdriverauthmode:

- $InputTCPServerStreamDriverAuthMode — maps to StreamDriver.AuthMode (status: legacy)

.. index::
   single: imtcp; $InputTCPServerStreamDriverAuthMode
   single: $InputTCPServerStreamDriverAuthMode

See also
--------
See also :doc:`../../configuration/modules/imtcp`.

