.. _param-imptcp-port:
.. _imptcp.parameter.input.port:

Port
====

.. index::
   single: imptcp; Port
   single: Port

.. summary-start

Selects the TCP port on which to listen.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imptcp`.

:Name: Port
:Scope: input
:Type: string
:Default: input=none
:Required?: no
:Introduced: at least 5.x, possibly earlier

Description
-----------
Select a port to listen on. It is an error to specify
both ``path`` and ``port``.

Input usage
-----------
.. _param-imptcp-input-port:
.. _imptcp.parameter.input.port-usage:

.. code-block:: rsyslog

   input(type="imptcp" port="...")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.

.. _imptcp.parameter.legacy.inputptcpserverrun:

- $InputPTCPServerRun — maps to Port (status: legacy)

.. index::
   single: imptcp; $InputPTCPServerRun
   single: $InputPTCPServerRun

See also
--------
See also :doc:`../../configuration/modules/imptcp`.
