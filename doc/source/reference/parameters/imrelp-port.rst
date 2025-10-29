.. _param-imrelp-port:
.. _imrelp.parameter.input.port:

Port
====

.. index::
   single: imrelp; Port
   single: Port

.. summary-start

Starts a RELP server instance that listens on the specified TCP port.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imrelp`.

:Name: Port
:Scope: input
:Type: string
:Default: input=none
:Required?: yes
:Introduced: Not documented

Description
-----------
Starts a RELP server on the selected port.

Input usage
-----------
.. _param-imrelp-input-port:
.. _imrelp.parameter.input.port-usage:

.. code-block:: rsyslog

   input(type="imrelp" port="2514")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.

.. _imrelp.parameter.legacy.inputrelpserverrun:

- $InputRELPServerRun — maps to Port (status: legacy)

.. index::
   single: imrelp; $InputRELPServerRun
   single: $InputRELPServerRun

See also
--------
See also :doc:`../../configuration/modules/imrelp`.
