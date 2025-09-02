.. _param-omsnmp-trapoid:
.. _omsnmp.parameter.module.trapoid:

TrapOID
=======

.. index::
   single: omsnmp; TrapOID
   single: TrapOID

.. summary-start

Defines the notification OID used for SNMPv2 traps.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omsnmp`.

:Name: TrapOID
:Scope: module
:Type: string
:Default: module=1.3.6.1.4.1.19406.1.2.1
:Required?: no
:Introduced: at least 7.3.0, possibly earlier

Description
-----------
The default value means "ADISCON-MONITORWARE-MIB::syslogtrap".

This configuration parameter is used for **SNMPv2** only.
This is the OID which defines the trap-type, or notification-type
rsyslog uses to send the trap.
In order to decode this OID, you will need to have the
ADISCON-MONITORWARE-MIB and ADISCON-MIB mibs installed on the
receiver side. Downloads of these mib files can be found here:

`https://www.adiscon.org/download/ADISCON-MIB.txt <https://www.adiscon.org/download/ADISCON-MIB.txt>`_

`https://www.adiscon.org/download/ADISCON-MONITORWARE-MIB.txt <https://www.adiscon.org/download/ADISCON-MONITORWARE-MIB.txt>`_
Thanks to the net-snmp mailinglist for the help and the
recommendations ;).

Module usage
------------
.. _param-omsnmp-module-trapoid:
.. _omsnmp.parameter.module.trapoid-usage:

.. code-block:: rsyslog

   action(type="omsnmp" trapOID="1.3.6.1.4.1.19406.1.2.1")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.

.. _omsnmp.parameter.legacy.actionsnmptrapoid:

- $actionsnmptrapoid — maps to TrapOID (status: legacy)

.. index::
   single: omsnmp; $actionsnmptrapoid
   single: $actionsnmptrapoid


See also
--------
See also :doc:`../../configuration/modules/omsnmp`.

