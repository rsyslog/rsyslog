.. _param-imdocker-pollinginterval:
.. _imdocker.parameter.module.pollinginterval:

.. index::
   single: imdocker; PollingInterval
   single: PollingInterval

.. summary-start

Sets how often to poll Docker for new containers in seconds.
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
Sets how often to poll the Docker engine for newly started containers.

.. _param-imdocker-module-pollinginterval:
.. _imdocker.parameter.module.pollinginterval-usage:

Module usage
------------
.. code-block:: rsyslog

   module(load="imdocker" PollingInterval="...")

See also
--------
See also :doc:`../../configuration/modules/imdocker`.
