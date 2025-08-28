.. _param-imtcp-streamdriver-prioritizesan:
.. _imtcp.parameter.module.streamdriver-prioritizesan:
.. _imtcp.parameter.input.streamdriver-prioritizesan:

StreamDriver.PrioritizeSAN
==========================

.. index::
   single: imtcp; StreamDriver.PrioritizeSAN
   single: StreamDriver.PrioritizeSAN

.. summary-start

Uses stricter SAN/CN matching for certificate validation.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imtcp`.

:Name: StreamDriver.PrioritizeSAN
:Scope: module, input
:Type: boolean
:Default: module=off, input=module parameter
:Required?: no
:Introduced: at least 5.x, possibly earlier

Description
-----------
Whether to use stricter SAN/CN matching. (driver-specific)

The same-named input parameter can override this module setting.


Module usage
------------
.. _param-imtcp-module-streamdriver-prioritizesan:
.. _imtcp.parameter.module.streamdriver-prioritizesan-usage:

.. code-block:: rsyslog

   module(load="imtcp" streamDriver.prioritizeSAN="on")

Input usage
-----------
.. _param-imtcp-input-streamdriver-prioritizesan:
.. _imtcp.parameter.input.streamdriver-prioritizesan-usage:

.. code-block:: rsyslog

   input(type="imtcp" port="514" streamDriver.prioritizeSAN="on")

See also
--------
See also :doc:`../../configuration/modules/imtcp`.

