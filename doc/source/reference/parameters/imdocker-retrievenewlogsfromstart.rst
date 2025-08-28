.. _param-imdocker-retrievenewlogsfromstart:
.. _imdocker.parameter.module.retrievenewlogsfromstart:

RetrieveNewLogsFromStart
========================

.. index::
   single: imdocker; RetrieveNewLogsFromStart
   single: RetrieveNewLogsFromStart

.. summary-start

Whether to read newly discovered container logs from start; default ``on``.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imdocker`.

:Name: RetrieveNewLogsFromStart
:Scope: module
:Type: boolean
:Default: module=on
:Required?: no
:Introduced: 8.41.0

Description
-----------
Specifies whether imdocker processes newly found container logs from the
beginning. Containers that exist when imdocker starts are controlled by
``GetContainerLogOptions`` and its ``tail`` option.

Module usage
------------
.. _param-imdocker-module-retrievenewlogsfromstart:
.. _imdocker.parameter.module.retrievenewlogsfromstart-usage:

.. code-block:: rsyslog

   module(load="imdocker" RetrieveNewLogsFromStart="...")

Notes
-----
- The original documentation described this as a binary option; it is a boolean parameter.

See also
--------
See also :doc:`../../configuration/modules/imdocker`.

