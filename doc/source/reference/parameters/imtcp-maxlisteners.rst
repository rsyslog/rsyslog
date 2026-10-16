.. _param-imtcp-maxlisteners:
.. _imtcp.parameter.module.maxlisteners:
.. _imtcp.parameter.input.maxlisteners:

MaxListeners
============

.. meta::
   :description: Limit imtcp listener-table capacity and resize it during bounded retained-listener reload.
   :keywords: rsyslog, imtcp, maxListeners, listener capacity, HUP, reload

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

With ``global(config.reloadOnHUP="on")``, the bounded retained-listener
reload can grow or shrink this capacity independently of ``maxSessions``.
The replacement pointer tables are allocated before the event-loop/worker
fence. Shrink is rejected if it would truncate an opened socket or occupied
listener slot; rejection leaves the active generation and sessions unchanged.
This does not add, remove or replace endpoints. See
:doc:`../../development/transactional_config_reload` for the retained limits.

YAML uses the same module and input keys:

.. code-block:: yaml

   modules:
     - load: imtcp
       maxListeners: 50
   inputs:
     - type: imtcp
       address: 127.0.0.1
       port: 514
       maxListeners: 20


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

