.. _param-imtcp-streamdriver-prioritizesan:
.. _imtcp.parameter.module.streamdriver-prioritizesan:

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
:Scope: module
:Type: boolean
:Default: module=off
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

See also
--------
See also :doc:`../../configuration/modules/imtcp`.

