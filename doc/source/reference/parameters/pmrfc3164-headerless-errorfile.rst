.. _param-pmrfc3164-headerless-errorfile:
.. _pmrfc3164.parameter.module.headerless-errorfile:
.. _pmrfc3164.parameter.module.headerless.errorfile:

headerless.errorfile
====================

.. index::
   single: pmrfc3164; headerless.errorfile
   single: headerless.errorfile

.. summary-start

Append raw headerless input to a file before other processing.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/pmrfc3164`.

:Name: headerless.errorfile
:Scope: module
:Type: string (see :doc:`../../rainerscript/constant_strings`)
:Default: module=none
:Required?: no
:Introduced: 8.2508.0

Description
-----------
If specified, raw headerless input is appended to this file before further processing. The file is created if it does not already exist.

Module usage
------------

.. _param-pmrfc3164-module-headerless-errorfile:
.. _pmrfc3164.parameter.module.headerless-errorfile-usage:
.. code-block:: rsyslog

   parser(name="custom.rfc3164" type="pmrfc3164" headerless.errorfile="/var/log/rsyslog-headerless.log")

See also
--------
See also :doc:`../../configuration/modules/pmrfc3164`.
