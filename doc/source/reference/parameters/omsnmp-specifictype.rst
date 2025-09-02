.. _param-omsnmp-specifictype:
.. _omsnmp.parameter.module.specifictype:

SpecificType
============

.. index::
   single: omsnmp; SpecificType
   single: SpecificType

.. summary-start

Defines the SNMPv1 specific trap number.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omsnmp`.

:Name: SpecificType
:Scope: module
:Type: integer
:Default: module=0
:Required?: no
:Introduced: at least 7.3.0, possibly earlier

Description
-----------
This is the specific trap number. This configuration parameter is
used for **SNMPv1** only. It has no effect if **SNMPv2** is used.

Module usage
------------
.. _param-omsnmp-module-specifictype:
.. _omsnmp.parameter.module.specifictype-usage:

.. code-block:: rsyslog

   action(type="omsnmp" specificType="0")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.

.. _omsnmp.parameter.legacy.actionsnmpspecifictype:

- $actionsnmpspecifictype — maps to SpecificType (status: legacy)

.. index::
   single: omsnmp; $actionsnmpspecifictype
   single: $actionsnmpspecifictype


See also
--------
See also :doc:`../../configuration/modules/omsnmp`.

