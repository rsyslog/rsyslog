.. _param-omsnmp-messageoid:
.. _omsnmp.parameter.module.messageoid:

MessageOID
==========

.. index::
   single: omsnmp; MessageOID
   single: MessageOID

.. summary-start

Specifies the OID for the message variable carrying the syslog text.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omsnmp`.

:Name: MessageOID
:Scope: module
:Type: string
:Default: module=1.3.6.1.4.1.19406.1.2.1
:Required?: no
:Introduced: at least 7.3.0, possibly earlier

Description
-----------
This OID will be used as a variable, type "OCTET STRING". This
variable will contain up to 255 characters of the original syslog
message including syslog header. It is recommend to use the default
OID.
In order to decode this OID, you will need to have the
ADISCON-MONITORWARE-MIB and ADISCON-MIB mibs installed on the
receiver side. To download these custom mibs, see the description of :ref:`param-omsnmp-trapoid`.

Module usage
------------
.. _param-omsnmp-module-messageoid:
.. _omsnmp.parameter.module.messageoid-usage:

.. code-block:: rsyslog

   action(type="omsnmp" messageOID="1.3.6.1.4.1.19406.1.2.1")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.

.. _omsnmp.parameter.legacy.actionsnmpsyslogmessageoid:

- $actionsnmpsyslogmessageoid — maps to MessageOID (status: legacy)

.. index::
   single: omsnmp; $actionsnmpsyslogmessageoid
   single: $actionsnmpsyslogmessageoid


See also
--------
See also :doc:`../../configuration/modules/omsnmp`.

