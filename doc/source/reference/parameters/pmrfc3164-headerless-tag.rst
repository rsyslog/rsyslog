.. _param-pmrfc3164-headerless-tag:
.. _pmrfc3164.parameter.module.headerless-tag:
.. _pmrfc3164.parameter.module.headerless.tag:

headerless.tag
==============

.. index::
   single: pmrfc3164; headerless.tag
   single: headerless.tag

.. summary-start

Set the tag used for headerless messages.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/pmrfc3164`.

:Name: headerless.tag
:Scope: module
:Type: string (see :doc:`../../rainerscript/constant_strings`)
:Default: module=headerless
:Required?: no
:Introduced: 8.2508.0

Description
-----------
Specifies the tag to assign to detected headerless messages.

Module usage
------------

.. _param-pmrfc3164-module-headerless-tag:
.. _pmrfc3164.parameter.module.headerless-tag-usage:
.. code-block:: rsyslog

   parser(name="custom.rfc3164" type="pmrfc3164" headerless.tag="missing")

See also
--------
See also :doc:`../../configuration/modules/pmrfc3164`.
