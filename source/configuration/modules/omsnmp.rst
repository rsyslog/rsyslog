*******************************
omsnmp: SNMP Trap Output Module
*******************************

===========================  ===========================================================================
**Module Name:**             **omsnmp**
**Author:**                  Andre Lorbach <alorbach@adiscon.com>
===========================  ===========================================================================


Purpose
=======

Provides the ability to send syslog messages as an SNMPv1 & v2c traps.
By default, SNMPv2c is preferred. The syslog message is wrapped into a
OCTED STRING variable. This module uses the
`NET-SNMP <http://net-snmp.sourceforge.net/>`_ library. In order to
compile this module, you will need to have the
`NET-SNMP <http://net-snmp.sourceforge.net/>`_ developer (headers)
package installed.


Configuration Parameters
========================

.. note::

   Parameter names are case-insensitive.

Action Parameters
-----------------

Server
^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "string", "none", "yes", "``$actionsnmptarget``"

This can be a hostname or ip address, and is our snmp target host.
This parameter is required, if the snmptarget is not defined, nothing
will be send.


Port
^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "162", "no", "``$actionsnmptargetport``"

The port which will be used, common values are port 162 or 161.


Transport
^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "string", "udp", "no", "``$actionsnmptransport``"

Defines the transport type you wish to use. Technically we can
support all transport types which are supported by NET-SNMP.
To name a few possible values:
udp, tcp, udp6, tcp6, icmp, icmp6 ...


Version
^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "1", "no", "``$actionsnmpversion``"

There can only be two choices for this parameter for now.
0 means SNMPv1 will be used.
1 means SNMPv2c will be used.
Any other value will default to 1.


Community
^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "string", "public", "no", "``$actionsnmpcommunity``"

This sets the used SNMP Community.


TrapOID
^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "string", "1.3.6.1.4.1.19406.1.2.1", "no", "``$actionsnmptrapoid``"

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


MessageOID
^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "string", "1.3.6.1.4.1.19406.1.2.1", "no", "``$actionsnmpsyslogmessageoid``"

This OID will be used as a variable, type "OCTET STRING". This
variable will contain up to 255 characters of the original syslog
message including syslog header. It is recommend to use the default
OID.
In order to decode this OID, you will need to have the
ADISCON-MONITORWARE-MIB and ADISCON-MIB mibs installed on the
receiver side. To download these custom mibs, see the description of
**TrapOID**.


EnterpriseOID
^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "string", "1.3.6.1.4.1.3.1.1", "no", "``$actionsnmpenterpriseoid``"

The default value means "enterprises.cmu.1.1"

Customize this value if needed. I recommend to use the default value
unless you require to use a different OID.
This configuration parameter is used for **SNMPv1** only. It has no
effect if **SNMPv2** is used.


SpecificType
^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "0", "no", "``$actionsnmpspecifictype``"

This is the specific trap number. This configuration parameter is
used for **SNMPv1** only. It has no effect if **SNMPv2** is used.


Snmpv1DynSource
^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "string", "", "no", "none"

.. versionadded:: 8.2001

If set, the source field of the SNMP trap can be overwritten with the a 
template. The internal default is "%fromhost-ip%". The result should be a 
valid IPv4 Address. Otherwise setting the source will fail.

Below is a sample template called "dynsource" which you canm use to set the 
source to a custom property:

.. code-block:: none

   set $!custom_host = $fromhost;
   template(name="dynsource" type="list") {
   	property(name="$!custom_host")
   }


This configuration parameter is used for **SNMPv1** only. 
It has no effect if **SNMPv2** is used.


TrapType
^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "6", "no", "``$actionsnmptraptype``"

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


Caveats/Known Bugs
==================

-  In order to decode the custom OIDs, you will need to have the adiscon
   mibs installed.


Examples
========

Sending messages as snmp traps
------------------------------

The following commands send every message as a snmp trap.

.. code-block:: none

   module(load="omsnmp")
   action(type="omsnmp" server="localhost" port="162" transport="udp"
          version="1" community="public")

