.. _param-imtcp-streamdriver-checkextendedkeypurpose:
.. _imtcp.parameter.module.streamdriver-checkextendedkeypurpose:
.. _imtcp.parameter.input.streamdriver-checkextendedkeypurpose:

StreamDriver.CheckExtendedKeyPurpose
====================================

.. index::
   single: imtcp; StreamDriver.CheckExtendedKeyPurpose
   single: StreamDriver.CheckExtendedKeyPurpose

.. summary-start

Checks the certificate's extended key usage (EKU) field for rsyslog compatibility.

.. summary-end

This parameter can be set at both the module and input levels.

:Name: StreamDriver.CheckExtendedKeyPurpose
:Scope: module, input
:Type: boolean
:Default: ``off`` (module), ``module parameter`` (input)
:Required?: no
:Introduced: at least 5.x, possibly earlier (module), 8.2106.0 (input)

Description
-----------
This parameter, when enabled, checks the purpose value in the extended key usage (EKU) part of a certificate to ensure it is compatible with rsyslog client/server operations. This is a driver-specific feature.

This can be set at the module level to define a global default for all ``imtcp`` listeners. It can also be overridden on a per-input basis.

Module usage
------------
.. _imtcp.parameter.module.streamdriver-checkextendedkeypurpose-usage:

.. code-block:: rsyslog

   module(load="imtcp"
          streamDriver.name="gtls"
          streamDriver.mode="1"
          streamDriver.checkExtendedKeyPurpose="on")

Input usage
-----------
.. _imtcp.parameter.input.streamdriver-checkextendedkeypurpose-usage:

.. code-block:: rsyslog

   input(type="imtcp"
         port="514"
         streamDriver.name="gtls"
         streamDriver.mode="1"
         streamDriver.checkExtendedKeyPurpose="on")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.

This parameter has no legacy name.

See also
--------
See also :doc:`../../configuration/modules/imtcp`.
