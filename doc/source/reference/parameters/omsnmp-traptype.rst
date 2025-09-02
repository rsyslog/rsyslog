.. _param-omsnmp-traptype:
.. _omsnmp.parameter.module.traptype:

TrapType
========

.. index::
   single: omsnmp; TrapType
   single: TrapType

.. summary-start

Selects the SNMPv1 generic trap type.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omsnmp`.

:Name: TrapType
:Scope: module
:Type: integer
:Default: module=6
:Required?: no
:Introduced: at least 7.3.0, possibly earlier

Description
-----------
There are only 7 Possible trap types defined which can be used here.
These trap types are:

.. code-block:: none

   0 = SNMP_TRAP_COLDSTART
   1 = SNMP_TRAP_WARMSTART
   2 = SNMP_TRAP_LINKDOWN
   3 = SNMP_TRAP_LINKUP
   4 = SNMP_TRAP_AUTHFAIL
   5 = SNMP_TRAP_EGPNEIGHBORLOSS
   6 = SNMP_TRAP_ENTERPRISESPECIFIC

.. note::

   Any other value will default to 6 automatically. This configuration
   parameter is used for **SNMPv1** only. It has no effect if **SNMPv2**
   is used.

Module usage
------------
.. _param-omsnmp-module-traptype:
.. _omsnmp.parameter.module.traptype-usage:

.. code-block:: rsyslog

   action(type="omsnmp" trapType="6")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.

.. _omsnmp.parameter.legacy.actionsnmptraptype:

- $actionsnmptraptype — maps to TrapType (status: legacy)

.. index::
   single: omsnmp; $actionsnmptraptype
   single: $actionsnmptraptype


See also
--------
See also :doc:`../../configuration/modules/omsnmp`.

