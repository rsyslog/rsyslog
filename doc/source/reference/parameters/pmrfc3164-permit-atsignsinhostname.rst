.. _param-pmrfc3164-permit-atsignsinhostname:
.. _pmrfc3164.parameter.module.permit-atsignsinhostname:
.. _pmrfc3164.parameter.module.permit.AtSignsInHostname:

permit.AtSignsInHostname
=========================

.. index::
   single: pmrfc3164; permit.AtSignsInHostname
   single: permit.AtSignsInHostname

.. summary-start

Allow ``@`` in hostnames, typically from syslog-ng relays.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/pmrfc3164`.

:Name: permit.AtSignsInHostname
:Scope: module
:Type: boolean
:Default: module=off
:Required?: no
:Introduced: 8.2508.0

Description
-----------
This option allows hostnames to include at-sign characters, which is useful when syslog-ng relays prefix source names with ``@``.

Module usage
------------

.. _param-pmrfc3164-module-permit-atsignsinhostname:
.. _pmrfc3164.parameter.module.permit-atsignsinhostname-usage:
.. code-block:: rsyslog

   parser(name="custom.rfc3164" type="pmrfc3164" permit.AtSignsInHostname="on")

Notes
-----
- Legacy docs referred to this as a ``binary`` option, which maps to a boolean.

See also
--------
See also :doc:`../../configuration/modules/pmrfc3164`.
