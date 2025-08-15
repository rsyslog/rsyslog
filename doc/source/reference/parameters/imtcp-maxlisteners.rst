.. _param-imtcp-maxlisteners:
.. _imtcp.parameter.module.maxlisteners:

MaxListeners
============

.. index::
   single: imtcp; MaxListeners
   single: MaxListeners

.. summary-start

Sets the maximum number of listener ports supported.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imtcp`.

:Name: MaxListeners
:Scope: module
:Type: integer
:Default: module=20
:Required?: no
:Introduced: at least 5.x, possibly earlier

Description
-----------
Sets the maximum number of listeners (server ports) supported.
This must be set before the first $InputTCPServerRun directive.

The same-named input parameter can override this module setting.


Module usage
------------
.. _param-imtcp-module-maxlisteners:
.. _imtcp.parameter.module.maxlisteners-usage:

.. code-block:: rsyslog

   module(load="imtcp" maxListeners="50")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.

.. _imtcp.parameter.legacy.inputtcpmaxlisteners:

- $InputTCPMaxListeners — maps to MaxListeners (status: legacy)

.. index::
   single: imtcp; $InputTCPMaxListeners
   single: $InputTCPMaxListeners

See also
--------
See also :doc:`../../configuration/modules/imtcp`.

