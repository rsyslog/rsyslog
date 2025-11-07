.. _param-omclickhouse-timeout:
.. _omclickhouse.parameter.input.timeout:

timeout
=======

.. index::
   single: omclickhouse; timeout
   single: timeout

.. summary-start

Configures the send timeout, in milliseconds, for ClickHouse HTTP requests.

.. summary-end

This parameter applies to :doc:`/configuration/modules/omclickhouse`.

:Name: timeout
:Scope: input
:Type: int (milliseconds)
:Default: 0
:Required?: no
:Introduced: not specified

Description
-----------
This parameter sets the timeout for sending data to ClickHouse. Value is
given in milliseconds.

Input usage
-----------
.. _omclickhouse.parameter.input.timeout-usage:

.. code-block:: rsyslog

   module(load="omclickhouse")
   action(type="omclickhouse" timeout="4500")

See also
--------
See also :doc:`/configuration/modules/omclickhouse`.
