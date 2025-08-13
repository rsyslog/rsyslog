.. _param-pmrfc3164-headerless-hostname:
.. _pmrfc3164.parameter.module.headerless-hostname:
.. _pmrfc3164.parameter.module.headerless.hostname:

headerless.hostname
===================

.. index::
   single: pmrfc3164; headerless.hostname
   single: headerless.hostname

.. summary-start

Override the hostname assigned to headerless messages.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/pmrfc3164`.

:Name: headerless.hostname
:Scope: module
:Type: string (see :doc:`../../rainerscript/constant_strings`)
:Default: module=<ip-address of sender>
:Required?: no
:Introduced: 8.2508.0

Description
-----------
By default, rsyslog uses the sender's IP address as the hostname for headerless messages. Set this option to provide a different hostname.

Module usage
------------

.. _param-pmrfc3164-module-headerless-hostname:
.. _pmrfc3164.parameter.module.headerless-hostname-usage:
.. code-block:: rsyslog

   parser(name="custom.rfc3164" type="pmrfc3164" headerless.hostname="example.local")

See also
--------
See also :doc:`../../configuration/modules/pmrfc3164`.
