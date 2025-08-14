.. _param-imtcp-streamdriver-authmode:
.. _imtcp.parameter.module.streamdriver-authmode:
.. _imtcp.parameter.input.streamdriver-authmode:

StreamDriver.AuthMode
=====================

.. index::
   single: imtcp; StreamDriver.AuthMode
   single: StreamDriver.AuthMode

.. summary-start

Sets the authentication mode for the selected network stream driver.

.. summary-end

This parameter can be set at both the module and input levels.

:Name: StreamDriver.AuthMode
:Scope: module, input
:Type: string
:Default: ``none`` (module), ``module parameter`` (input)
:Required?: no
:Introduced: at least 5.x, possibly earlier (module), 8.2106.0 (input)

Description
-----------
This parameter sets the authentication mode for the stream driver. The possible values and their meanings depend on the selected :doc:`network stream driver <../../concepts/netstrm_drvr>`. For example, for the ``gtls`` driver, common modes include ``anon``, ``x509/fingerprint``, and ``x509/name``.

This can be set at the module level to define a global default for all ``imtcp`` listeners. It can also be overridden on a per-input basis.

Module usage
------------
.. _imtcp.parameter.module.streamdriver-authmode-usage:

.. code-block:: rsyslog

   module(load="imtcp"
          streamDriver.name="gtls"
          streamDriver.mode="1"
          streamDriver.authMode="x509/name")

Input usage
-----------
.. _imtcp.parameter.input.streamdriver-authmode-usage:

.. code-block:: rsyslog

   input(type="imtcp"
         port="514"
         streamDriver.name="gtls"
         streamDriver.mode="1"
         streamDriver.authMode="x509/name")

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
