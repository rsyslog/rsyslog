.. _param-omsnmp-community:
.. _omsnmp.parameter.module.community:

Community
=========

.. index::
   single: omsnmp; Community
   single: Community

.. summary-start

Sets the SNMP community string.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omsnmp`.

:Name: Community
:Scope: module
:Type: string
:Default: module=public
:Required?: no
:Introduced: at least 7.3.0, possibly earlier

Description
-----------
This sets the used SNMP Community.

Module usage
------------
.. _param-omsnmp-module-community:
.. _omsnmp.parameter.module.community-usage:

.. code-block:: rsyslog

   action(type="omsnmp" community="public")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.

.. _omsnmp.parameter.legacy.actionsnmpcommunity:

- $actionsnmpcommunity — maps to Community (status: legacy)

.. index::
   single: omsnmp; $actionsnmpcommunity
   single: $actionsnmpcommunity


See also
--------
See also :doc:`../../configuration/modules/omsnmp`.

