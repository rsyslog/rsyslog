.. _param-omsnmp-port:
.. _omsnmp.parameter.module.port:

Port
====

.. index::
   single: omsnmp; Port
   single: Port

.. summary-start

Specifies the port used for sending SNMP traps.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omsnmp`.

:Name: Port
:Scope: module
:Type: integer
:Default: module=162
:Required?: no
:Introduced: at least 7.3.0, possibly earlier

Description
-----------
The port which will be used, common values are port 162 or 161.

Module usage
------------
.. _param-omsnmp-module-port:
.. _omsnmp.parameter.module.port-usage:

.. code-block:: rsyslog

   action(type="omsnmp" port="162")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.

.. _omsnmp.parameter.legacy.actionsnmptargetport:

- $actionsnmptargetport — maps to Port (status: legacy)

.. index::
   single: omsnmp; $actionsnmptargetport
   single: $actionsnmptargetport


See also
--------
See also :doc:`../../configuration/modules/omsnmp`.

