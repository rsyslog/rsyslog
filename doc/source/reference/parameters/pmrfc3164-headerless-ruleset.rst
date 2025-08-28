.. _param-pmrfc3164-headerless-ruleset:
.. _pmrfc3164.parameter.module.headerless-ruleset:
.. _pmrfc3164.parameter.module.headerless.ruleset:

headerless.ruleset
==================

.. index::
   single: pmrfc3164; headerless.ruleset
   single: headerless.ruleset

.. summary-start

Route headerless messages to a specific ruleset.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/pmrfc3164`.

:Name: headerless.ruleset
:Scope: module
:Type: string (see :doc:`../../rainerscript/constant_strings`)
:Default: module=none
:Required?: no
:Introduced: 8.2508.0

Description
-----------
If set, detected headerless messages are sent to the given ruleset for further processing, such as writing to a dedicated error log or discarding.

Module usage
------------

.. _param-pmrfc3164-module-headerless-ruleset:
.. _pmrfc3164.parameter.module.headerless-ruleset-usage:
.. code-block:: rsyslog

   parser(name="custom.rfc3164" type="pmrfc3164" headerless.ruleset="errorhandler")

See also
--------
See also :doc:`../../configuration/modules/pmrfc3164`.
