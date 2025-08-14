.. _param-imtcp-streamdriver-name:
.. _imtcp.parameter.module.streamdriver-name:
.. _imtcp.parameter.input.streamdriver-name:

StreamDriver.Name
=================

.. index::
   single: imtcp; StreamDriver.Name
   single: StreamDriver.Name

.. summary-start

Selects the network stream driver to be used for network communication.

.. summary-end

This parameter can be set at both the module and input levels.

:Name: StreamDriver.Name
:Scope: module, input
:Type: string
:Default: ``none`` (module), ``module parameter`` (input)
:Required?: no
:Introduced: at least 5.x, possibly earlier (module), 8.2106.0 (input)

Description
-----------
This parameter selects the :doc:`network stream driver <../../concepts/netstrm_drvr>` to be used for all inputs defined by this module. The selected driver handles the underlying network communication, allowing for features like TLS encryption.

This can be set at the module level to define a global default for all ``imtcp`` listeners. It can also be overridden on a per-input basis.

Module usage
------------
.. _imtcp.parameter.module.streamdriver-name-usage:

.. code-block:: rsyslog

   module(load="imtcp" streamDriver.name="gtls")

Input usage
-----------
.. _imtcp.parameter.input.streamdriver-name-usage:

.. code-block:: rsyslog

   input(type="imtcp" port="514" streamDriver.name="gtls")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.

This parameter has no legacy name.

See also
--------
See also :doc:`../../configuration/modules/imtcp`.
