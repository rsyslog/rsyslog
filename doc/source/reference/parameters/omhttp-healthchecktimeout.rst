.. _param-omhttp-healthchecktimeout:
.. _omhttp.parameter.module.healthchecktimeout:

healthchecktimeout
==================

.. index::
   single: omhttp; healthchecktimeout
   single: healthchecktimeout

.. summary-start

Sets the number of milliseconds omhttp waits before a health check request times out.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omhttp`.

:Name: healthchecktimeout
:Scope: module
:Type: integer
:Default: module=3500
:Required?: no
:Introduced: Not specified

Description
-----------
The time after which the health check will time out in milliseconds.

Module usage
------------
.. _omhttp.parameter.module.healthchecktimeout-usage:

.. code-block:: rsyslog

   module(load="omhttp")

   action(
       type="omhttp"
       healthCheckTimeout="5000"
   )

See also
--------
See also :doc:`../../configuration/modules/omhttp`.
