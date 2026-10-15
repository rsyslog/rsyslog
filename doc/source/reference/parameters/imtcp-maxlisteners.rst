.. _param-imtcp-maxlisteners:
.. _imtcp.parameter.module.maxlisteners:
.. _imtcp.parameter.input.maxlisteners:

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
:Scope: module, input
:Type: integer
:Default: module=20, input=module parameter
:Required?: no
:Introduced: at least 5.x, possibly earlier

Description
-----------
Sets the maximum number of listeners (server ports) supported.
This must be set before the first $InputTCPServerRun directive.

The same-named input parameter can override this module setting.

When transactional reload is enabled, changing this capacity is live if the
new value still accommodates every socket opened for the input.  The listener
sockets and established sessions remain active while preallocated pointer
tables are published at the imtcp reload fence.  A shrink below the current
number of opened listeners is rejected without changing the active generation.


Module usage
------------
.. _param-imtcp-module-maxlisteners:
.. _imtcp.parameter.module.maxlisteners-usage:

.. code-block:: rsyslog

   module(load="imtcp" maxListeners="50")

Input usage
-----------
.. _param-imtcp-input-maxlisteners:
.. _imtcp.parameter.input.maxlisteners-usage:

.. code-block:: rsyslog

   input(type="imtcp" port="514" maxListeners="50")

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
