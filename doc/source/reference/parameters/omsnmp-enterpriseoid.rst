.. _param-omsnmp-enterpriseoid:
.. _omsnmp.parameter.module.enterpriseoid:

EnterpriseOID
=============

.. index::
   single: omsnmp; EnterpriseOID
   single: EnterpriseOID

.. summary-start

Specifies the enterprise OID used in SNMPv1 traps.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omsnmp`.

:Name: EnterpriseOID
:Scope: module
:Type: string
:Default: module=1.3.6.1.4.1.3.1.1
:Required?: no
:Introduced: at least 7.3.0, possibly earlier

Description
-----------
The default value means "enterprises.cmu.1.1"

Customize this value if needed. I recommend to use the default value
unless you require to use a different OID.
This configuration parameter is used for **SNMPv1** only. It has no
effect if **SNMPv2** is used.

Module usage
------------
.. _param-omsnmp-module-enterpriseoid:
.. _omsnmp.parameter.module.enterpriseoid-usage:

.. code-block:: rsyslog

   action(type="omsnmp" enterpriseOID="1.3.6.1.4.1.3.1.1")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.

.. _omsnmp.parameter.legacy.actionsnmpenterpriseoid:

- $actionsnmpenterpriseoid — maps to EnterpriseOID (status: legacy)

.. index::
   single: omsnmp; $actionsnmpenterpriseoid
   single: $actionsnmpenterpriseoid


See also
--------
See also :doc:`../../configuration/modules/omsnmp`.

