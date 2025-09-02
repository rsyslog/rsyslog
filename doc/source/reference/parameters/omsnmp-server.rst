.. _param-omsnmp-server:
.. _omsnmp.parameter.module.server:

Server
======

.. index::
   single: omsnmp; Server
   single: Server

.. summary-start

Defines the SNMP target host by hostname or IP address.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omsnmp`.

:Name: Server
:Scope: module
:Type: string
:Default: module=none
:Required?: yes
:Introduced: at least 7.3.0, possibly earlier

Description
-----------
This can be a hostname or ip address, and is our snmp target host.
This parameter is required, if the snmptarget is not defined, nothing
will be send.

Module usage
------------
.. _param-omsnmp-module-server:
.. _omsnmp.parameter.module.server-usage:

.. code-block:: rsyslog

   action(type="omsnmp" server="192.0.2.1")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.

.. _omsnmp.parameter.legacy.actionsnmptarget:

- $actionsnmptarget — maps to Server (status: legacy)

.. index::
   single: omsnmp; $actionsnmptarget
   single: $actionsnmptarget


See also
--------
See also :doc:`../../configuration/modules/omsnmp`.

