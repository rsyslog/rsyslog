.. _param-pmrfc3164-permit-slashesinhostname:
.. _pmrfc3164.parameter.module.permit-slashesinhostname:
.. _pmrfc3164.parameter.module.permit.slashesInHostname:

permit.slashesInHostname
========================

.. index::
   single: pmrfc3164; permit.slashesInHostname
   single: permit.slashesInHostname

.. summary-start

Allow ``/`` in hostnames, useful for syslog-ng relay chains.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/pmrfc3164`.

:Name: permit.slashesInHostname
:Scope: module
:Type: boolean
:Default: module=off
:Required?: no
:Introduced: 8.20.0

Description
-----------
This option allows hostnames to include slash characters.

Module usage
------------

.. _param-pmrfc3164-module-permit-slashesinhostname:
.. _pmrfc3164.parameter.module.permit-slashesinhostname-usage:
.. code-block:: rsyslog

   parser(name="custom.rfc3164" type="pmrfc3164" permit.slashesInHostname="on")

Notes
-----
- Legacy docs referred to this as a ``binary`` option, which maps to a boolean.

See also
--------
See also :doc:`../../configuration/modules/pmrfc3164`.
