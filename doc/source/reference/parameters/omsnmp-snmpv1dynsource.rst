.. _param-omsnmp-snmpv1dynsource:
.. _omsnmp.parameter.module.snmpv1dynsource:

Snmpv1DynSource
===============

.. index::
   single: omsnmp; Snmpv1DynSource
   single: Snmpv1DynSource

.. summary-start

Overrides the SNMPv1 trap source address via template.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omsnmp`.

:Name: Snmpv1DynSource
:Scope: module
:Type: string
:Default: module=""
:Required?: no
:Introduced: 8.2001

Description
-----------
.. versionadded:: 8.2001

If set, the source field of the SNMP trap can be overwritten with the a
template. The internal default is "%fromhost-ip%". The result should be a
valid IPv4 Address. Otherwise setting the source will fail.

Below is a sample template called "dynsource" which you can use to set the
source to a custom property:

.. code-block:: none

   set $!custom_host = $fromhost;
   template(name="dynsource" type="list") {
        property(name="$!custom_host")
   }


This configuration parameter is used for **SNMPv1** only.
It has no effect if **SNMPv2** is used.

Module usage
------------
.. _param-omsnmp-module-snmpv1dynsource:
.. _omsnmp.parameter.module.snmpv1dynsource-usage:

.. code-block:: rsyslog

   action(type="omsnmp" snmpv1DynSource="dynsource")

See also
--------
See also :doc:`../../configuration/modules/omsnmp`.

