.. _param-omsnmp-transport:
.. _omsnmp.parameter.module.transport:

Transport
=========

.. index::
   single: omsnmp; Transport
   single: Transport

.. summary-start

Selects the transport protocol for SNMP communication.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omsnmp`.

:Name: Transport
:Scope: module
:Type: string
:Default: module=udp
:Required?: no
:Introduced: at least 7.3.0, possibly earlier

Description
-----------
Defines the transport type you wish to use. Technically we can
support all transport types which are supported by NET-SNMP.
To name a few possible values:
udp, tcp, udp6, tcp6, icmp, icmp6 ...

Module usage
------------
.. _param-omsnmp-module-transport:
.. _omsnmp.parameter.module.transport-usage:

.. code-block:: rsyslog

   action(type="omsnmp" transport="udp")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.

.. _omsnmp.parameter.legacy.actionsnmptransport:

- $actionsnmptransport — maps to Transport (status: legacy)

.. index::
   single: omsnmp; $actionsnmptransport
   single: $actionsnmptransport


See also
--------
See also :doc:`../../configuration/modules/omsnmp`.

