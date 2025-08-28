.. _param-imtcp-port:
.. _imtcp.parameter.input.port:

Port
====

.. index::
   single: imtcp; Port
   single: Port

.. summary-start

Starts a TCP server on the specified port.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imtcp`.

:Name: Port
:Scope: input
:Type: string (see :doc:`../../rainerscript/constant_strings`)
:Default: input=none
:Required?: yes
:Introduced: at least 5.x, possibly earlier

Description
-----------
Starts a TCP server on the selected port. If port zero is selected, the OS automatically
assigns a free port. Use ``listenPortFileName`` to learn which port was assigned.

Input usage
-----------
.. _param-imtcp-input-port:
.. _imtcp.parameter.input.port-usage:

.. code-block:: rsyslog

   input(type="imtcp" port="514")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.

.. _imtcp.parameter.legacy.inputtcpserverrun:

- $InputTCPServerRun — maps to Port (status: legacy)

.. index::
   single: imtcp; $InputTCPServerRun
   single: $InputTCPServerRun

See also
--------
See also :doc:`../../configuration/modules/imtcp`.
