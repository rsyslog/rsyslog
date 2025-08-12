.. _param-imdocker-pollinginterval:
.. _imdocker.parameter.module.pollinginterval:

PollingInterval
===============

.. index::
   single: imdocker; PollingInterval
   single: PollingInterval

.. summary-start

Seconds between ``List Containers`` polls for new containers; default ``60``.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imdocker`.

:Name: PollingInterval
:Scope: module
:Type: integer
:Default: module=60
:Required?: no
:Introduced: 8.41.0

Description
-----------
Specifies the polling interval in seconds. imdocker polls for new containers by
calling the ``List containers`` Docker API.

Module usage
------------
.. _param-imdocker-module-pollinginterval:
.. _imdocker.parameter.module.pollinginterval-usage:

.. code-block:: rsyslog

   module(load="imdocker" PollingInterval="...")

See also
--------
See also :doc:`../../configuration/modules/imdocker`.

