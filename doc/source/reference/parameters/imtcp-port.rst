.. _param-imtcp-port:
.. _imtcp.parameter.input.port:

Port
====

.. index::
   single: imtcp; Port
   single: Port

.. summary-start

Specifies the TCP port to listen on for incoming messages.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imtcp`.

:Name: Port
:Scope: input
:Type: string
:Default: none
:Required?: yes
:Introduced: at least 5.x, possibly earlier

Description
-----------
This parameter instructs ``imtcp`` to start a TCP server on the selected port. If port zero is selected, the operating system will automatically assign a free port. In that case, the ``listenPortFileName`` parameter can be used to obtain the information of which port was assigned.

Input usage
-----------
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
