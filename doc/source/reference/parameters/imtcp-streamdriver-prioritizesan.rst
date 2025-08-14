.. _param-imtcp-streamdriver-prioritizesan:
.. _imtcp.parameter.module.streamdriver-prioritizesan:
.. _imtcp.parameter.input.streamdriver-prioritizesan:

StreamDriver.PrioritizeSAN
==========================

.. index::
   single: imtcp; StreamDriver.PrioritizeSAN
   single: StreamDriver.PrioritizeSAN

.. summary-start

Enables stricter SAN/CN matching for TLS certificates.

.. summary-end

This parameter can be set at both the module and input levels.

:Name: StreamDriver.PrioritizeSAN
:Scope: module, input
:Type: boolean
:Default: ``off`` (module), ``module parameter`` (input)
:Required?: no
:Introduced: at least 5.x, possibly earlier (module), 8.2106.0 (input)

Description
-----------
This parameter controls whether to use stricter matching of the Subject Alternative Name (SAN) and Common Name (CN) fields in TLS certificates. This is a driver-specific feature.

This can be set at the module level to define a global default for all ``imtcp`` listeners. It can also be overridden on a per-input basis.

Module usage
------------
.. _imtcp.parameter.module.streamdriver-prioritizesan-usage:

.. code-block:: rsyslog

   module(load="imtcp"
          streamDriver.name="gtls"
          streamDriver.mode="1"
          streamDriver.prioritizeSAN="on")

Input usage
-----------
.. _imtcp.parameter.input.streamdriver-prioritizesan-usage:

.. code-block:: rsyslog

   input(type="imtcp"
         port="514"
         streamDriver.name="gtls"
         streamDriver.mode="1"
         streamDriver.prioritizeSAN="on")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.

This parameter has no legacy name.

See also
--------
See also :doc:`../../configuration/modules/imtcp`.
