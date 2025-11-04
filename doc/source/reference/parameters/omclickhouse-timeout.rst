.. _param-omclickhouse-timeout:
.. _omclickhouse.parameter.module.timeout:

timeout
=======

.. index::
   single: omclickhouse; timeout
   single: timeout

.. summary-start

Configures the send timeout, in milliseconds, for ClickHouse HTTP requests.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omclickhouse`.

:Name: timeout
:Scope: module
:Type: int (milliseconds)
:Default: module=0
:Required?: no
:Introduced: not specified

Description
-----------
This parameter sets the timeout for sending data to ClickHouse. Value is given in milliseconds.

Module usage
------------
.. _param-omclickhouse-module-timeout:
.. _omclickhouse.parameter.module.timeout-usage:

.. code-block:: rsyslog

   module(load="omclickhouse")
   action(type="omclickhouse" timeout="4500")

See also
--------
See also :doc:`../../configuration/modules/omclickhouse`.
