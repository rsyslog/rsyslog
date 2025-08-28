.. _param-pmrfc3164-permit-squarebracketsinhostname:
.. _pmrfc3164.parameter.module.permit-squarebracketsinhostname:
.. _pmrfc3164.parameter.module.permit.squareBracketsInHostname:

permit.squareBracketsInHostname
================================

.. index::
   single: pmrfc3164; permit.squareBracketsInHostname
   single: permit.squareBracketsInHostname

.. summary-start

Accept hostnames enclosed in ``[]``.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/pmrfc3164`.

:Name: permit.squareBracketsInHostname
:Scope: module
:Type: boolean
:Default: module=off
:Required?: no
:Introduced: at least 8.x, possibly earlier

Description
-----------
This option allows hostnames wrapped in square brackets to be parsed without the brackets.

Module usage
------------

.. _param-pmrfc3164-module-permit-squarebracketsinhostname:
.. _pmrfc3164.parameter.module.permit-squarebracketsinhostname-usage:
.. code-block:: rsyslog

   parser(name="custom.rfc3164" type="pmrfc3164" permit.squareBracketsInHostname="on")

Notes
-----
- Legacy docs referred to this as a ``binary`` option, which maps to a boolean.

See also
--------
See also :doc:`../../configuration/modules/pmrfc3164`.
