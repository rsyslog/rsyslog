.. _param-imudp-port:
.. _imudp.parameter.input.port:

Port
====

.. index::
   single: imudp; Port
   single: Port

.. summary-start

Port or array of ports for the UDP listener.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imudp`.

:Name: Port
:Scope: input
:Type: array[string]
:Default: input=514
:Required?: yes
:Introduced: at least 5.x, possibly earlier

Description
-----------
Specifies the port the server shall listen to. Either a single port can be
specified or an array of ports. If multiple ports are specified, a listener will
be automatically started for each port. Thus, no additional inputs need to be
configured.

Examples:

.. code-block:: rsyslog

   module(load="imudp") # needs to be done just once
   input(type="imudp" port="514")

.. code-block:: rsyslog

   module(load="imudp")
   input(type="imudp" port=["514","515","10514"])

Input usage
-----------
.. _param-imudp-input-port:
.. _imudp.parameter.input.port-usage:

.. code-block:: rsyslog

   input(type="imudp" Port="...")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.

.. _imudp.parameter.legacy.udpserverrun:

- $UDPServerRun — maps to Port (status: legacy)

.. index::
   single: imudp; $UDPServerRun
   single: $UDPServerRun

See also
--------
See also :doc:`../../configuration/modules/imudp`.
