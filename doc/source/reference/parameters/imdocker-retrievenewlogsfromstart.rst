.. _param-imdocker-retrievenewlogsfromstart:
.. _imdocker.parameter.module.retrievenewlogsfromstart:

.. index::
   single: imdocker; RetrieveNewLogsFromStart
   single: RetrieveNewLogsFromStart

.. summary-start

Controls whether newly discovered containers are read from the start of their logs.
.. summary-end

This parameter applies to :doc:`../../configuration/modules/imdocker`.

:Name: RetrieveNewLogsFromStart
:Scope: module
:Type: boolean
:Default: module=1
:Required?: no
:Introduced: 8.41.0

Description
-----------
Controls whether logs from containers found after start are processed from their beginning. Containers present at module start are governed by ``GetContainerLogOptions``.

.. _param-imdocker-module-retrievenewlogsfromstart:
.. _imdocker.parameter.module.retrievenewlogsfromstart-usage:

Module usage
------------
.. code-block:: rsyslog

   module(load="imdocker" RetrieveNewLogsFromStart="...")

Notes
-----
This parameter was historically described as ``binary`` but acts as a boolean option.

See also
--------
See also :doc:`../../configuration/modules/imdocker`.
