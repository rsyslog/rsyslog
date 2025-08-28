.. _param-pmrfc3164-remove-msgfirstspace:
.. _pmrfc3164.parameter.module.remove-msgfirstspace:
.. _pmrfc3164.parameter.module.remove.msgFirstSpace:

remove.msgFirstSpace
====================

.. index::
   single: pmrfc3164; remove.msgFirstSpace
   single: remove.msgFirstSpace

.. summary-start

Remove the first space after the tag to improve RFC3164/RFC5424 interoperability.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/pmrfc3164`.

:Name: remove.msgFirstSpace
:Scope: module
:Type: boolean
:Default: module=off
:Required?: no
:Introduced: 8.2508.0

Description
-----------
RFC3164 places a space after the tag. When enabled, this option removes that first space so RFC3164 and RFC5424 messages can mix more easily.

Module usage
------------

.. _param-pmrfc3164-module-remove-msgfirstspace:
.. _pmrfc3164.parameter.module.remove-msgfirstspace-usage:
.. code-block:: rsyslog

   parser(name="custom.rfc3164" type="pmrfc3164" remove.msgFirstSpace="on")

Notes
-----
- Legacy docs referred to this as a ``binary`` option, which maps to a boolean.

See also
--------
See also :doc:`../../configuration/modules/pmrfc3164`.
