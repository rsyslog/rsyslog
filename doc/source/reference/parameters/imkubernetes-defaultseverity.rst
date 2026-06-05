.. _param-imkubernetes-defaultseverity:
.. _imkubernetes.parameter.module.defaultseverity:

DefaultSeverity
===============

.. index::
   single: imkubernetes; DefaultSeverity
   single: DefaultSeverity

.. summary-start

Default severity assigned to submitted records that are not mapped to
``stderr``; default ``info``.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imkubernetes`.

:Name: DefaultSeverity
:Scope: module
:Type: severity
:Default: ``info``
:Required?: no
:Introduced: 8.2604.0

Description
-----------
``imkubernetes`` uses ``err`` for parsed ``stderr`` records. For other records,
including ``stdout`` and raw fallback lines, this setting controls the emitted
syslog severity.

Module usage
------------

.. code-block:: rsyslog

   module(load="imkubernetes" DefaultSeverity="notice")

See also
--------
See also :doc:`../../configuration/modules/imkubernetes`.
