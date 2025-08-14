.. _param-imtcp-streamdriver-permitexpiredcerts:
.. _imtcp.parameter.module.streamdriver-permitexpiredcerts:
.. _imtcp.parameter.input.streamdriver-permitexpiredcerts:

StreamDriver.PermitExpiredCerts
===============================

.. index::
   single: imtcp; StreamDriver.PermitExpiredCerts
   single: StreamDriver.PermitExpiredCerts

.. summary-start

Controls how the TLS stream driver handles expired certificates.

.. summary-end

This parameter can be set at both the module and input levels.

:Name: StreamDriver.PermitExpiredCerts
:Scope: module, input
:Type: string
:Default: ``off`` (module), ``module parameter`` (input)
:Required?: no
:Introduced: at least 5.x, possibly earlier (module), 8.2106.0 (input)

.. versionchanged:: 8.2012.0
   The module-level default was changed from ``warn`` to ``off``.

Description
-----------
This parameter controls how expired certificates are handled when the stream driver is operating in TLS mode. It can have one of the following values:

-  **on**: Expired certificates are allowed.
-  **off**: Expired certificates are not allowed. This is the default since version 8.2012.0.
-  **warn**: Expired certificates are allowed, but a warning message will be logged. This was the default prior to version 8.2012.0.

This can be set at the module level to define a global default for all ``imtcp`` listeners. It can also be overridden on a per-input basis.

Module usage
------------
.. _imtcp.parameter.module.streamdriver-permitexpiredcerts-usage:

.. code-block:: rsyslog

   module(load="imtcp"
          streamDriver.name="gtls"
          streamDriver.mode="1"
          streamDriver.permitExpiredCerts="on")

Input usage
-----------
.. _imtcp.parameter.input.streamdriver-permitexpiredcerts-usage:

.. code-block:: rsyslog

   input(type="imtcp"
         port="514"
         streamDriver.name="gtls"
         streamDriver.mode="1"
         streamDriver.permitExpiredCerts="warn")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.

This parameter has no legacy name.

See also
--------
See also :doc:`../../configuration/modules/imtcp`.
