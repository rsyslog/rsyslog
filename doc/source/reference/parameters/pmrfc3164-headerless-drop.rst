.. _param-pmrfc3164-headerless-drop:
.. _pmrfc3164.parameter.module.headerless-drop:
.. _pmrfc3164.parameter.module.headerless.drop:

headerless.drop
===============

.. index::
   single: pmrfc3164; headerless.drop
   single: headerless.drop

.. summary-start

Discard headerless messages after optional logging.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/pmrfc3164`.

:Name: headerless.drop
:Scope: module
:Type: boolean
:Default: module=off
:Required?: no
:Introduced: 8.2508.0

Description
-----------
When enabled, headerless messages are discarded after optional logging to ``headerless.errorfile`` and are not processed by later rules.

Module usage
------------

.. _param-pmrfc3164-module-headerless-drop:
.. _pmrfc3164.parameter.module.headerless-drop-usage:
.. code-block:: rsyslog

   parser(name="custom.rfc3164" type="pmrfc3164" headerless.drop="on")

Notes
-----
- Legacy docs referred to this as a ``binary`` option, which maps to a boolean.

See also
--------
See also :doc:`../../configuration/modules/pmrfc3164`.
