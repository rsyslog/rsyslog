.. _param-omsnmp-version:
.. _omsnmp.parameter.module.version:

Version
=======

.. index::
   single: omsnmp; Version
   single: Version

.. summary-start

Chooses whether SNMPv1 or SNMPv2c traps are sent.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omsnmp`.

:Name: Version
:Scope: module
:Type: integer
:Default: module=1
:Required?: no
:Introduced: at least 7.3.0, possibly earlier

Description
-----------
There can only be two choices for this parameter for now.
0 means SNMPv1 will be used.
1 means SNMPv2c will be used.
Any other value will default to 1.

Module usage
------------
.. _param-omsnmp-module-version:
.. _omsnmp.parameter.module.version-usage:

.. code-block:: rsyslog

   action(type="omsnmp" version="1")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.

.. _omsnmp.parameter.legacy.actionsnmpversion:

- $actionsnmpversion — maps to Version (status: legacy)

.. index::
   single: omsnmp; $actionsnmpversion
   single: $actionsnmpversion


See also
--------
See also :doc:`../../configuration/modules/omsnmp`.

