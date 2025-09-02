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

   Parameter names are case-insensitive; camelCase is recommended for readability.

Action Parameters
-----------------

.. list-table::
   :widths: 30 70
   :header-rows: 1

   * - Parameter
     - Summary
   * - :ref:`param-omsnmp-server`
     - .. include:: ../../reference/parameters/omsnmp-server.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omsnmp-port`
     - .. include:: ../../reference/parameters/omsnmp-port.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omsnmp-transport`
     - .. include:: ../../reference/parameters/omsnmp-transport.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omsnmp-version`
     - .. include:: ../../reference/parameters/omsnmp-version.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omsnmp-community`
     - .. include:: ../../reference/parameters/omsnmp-community.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omsnmp-trapoid`
     - .. include:: ../../reference/parameters/omsnmp-trapoid.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omsnmp-messageoid`
     - .. include:: ../../reference/parameters/omsnmp-messageoid.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omsnmp-enterpriseoid`
     - .. include:: ../../reference/parameters/omsnmp-enterpriseoid.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omsnmp-specifictype`
     - .. include:: ../../reference/parameters/omsnmp-specifictype.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omsnmp-snmpv1dynsource`
     - .. include:: ../../reference/parameters/omsnmp-snmpv1dynsource.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omsnmp-traptype`
     - .. include:: ../../reference/parameters/omsnmp-traptype.rst
        :start-after: .. summary-start
        :end-before: .. summary-end

.. toctree::
   :hidden:

   ../../reference/parameters/omsnmp-server
   ../../reference/parameters/omsnmp-port
   ../../reference/parameters/omsnmp-transport
   ../../reference/parameters/omsnmp-version
   ../../reference/parameters/omsnmp-community
   ../../reference/parameters/omsnmp-trapoid
   ../../reference/parameters/omsnmp-messageoid
   ../../reference/parameters/omsnmp-enterpriseoid
   ../../reference/parameters/omsnmp-specifictype
   ../../reference/parameters/omsnmp-snmpv1dynsource
   ../../reference/parameters/omsnmp-traptype

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

