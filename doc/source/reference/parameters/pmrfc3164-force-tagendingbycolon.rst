.. _param-pmrfc3164-force-tagendingbycolon:
.. _pmrfc3164.parameter.module.force-tagendingbycolon:
.. _pmrfc3164.parameter.module.force.tagEndingByColon:

force.tagEndingByColon
======================

.. index::
   single: pmrfc3164; force.tagEndingByColon
   single: force.tagEndingByColon

.. summary-start

Require tags to end with a colon or set the tag to ``-``.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/pmrfc3164`.

:Name: force.tagEndingByColon
:Scope: module
:Type: boolean
:Default: module=off
:Required?: no
:Introduced: 8.2508.0

Description
-----------
When enabled, tags must end with a colon to be valid. Otherwise the tag is replaced with ``-`` without modifying the message.

Module usage
------------

.. _param-pmrfc3164-module-force-tagendingbycolon:
.. _pmrfc3164.parameter.module.force-tagendingbycolon-usage:
.. code-block:: rsyslog

   parser(name="custom.rfc3164" type="pmrfc3164" force.tagEndingByColon="on")

Notes
-----
- Legacy docs referred to this as a ``binary`` option, which maps to a boolean.

See also
--------
See also :doc:`../../configuration/modules/pmrfc3164`.
