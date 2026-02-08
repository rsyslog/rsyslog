.. _param-impstats-push-timeout-ms:
.. _impstats.parameter.module.push-timeout-ms:

push.timeout.ms
===============

.. index::
   single: impstats; push.timeout.ms
   single: push.timeout.ms

.. summary-start

Sets the HTTP request timeout for Remote Write push (milliseconds).

.. summary-end

This parameter applies to :doc:`../../configuration/modules/impstats`.

:Name: push.timeout.ms
:Scope: module
:Type: integer (milliseconds)
:Default: module=5000
:Required?: no
:Introduced: 8.2602.0

Description
-----------
Configures the maximum time allowed for the HTTP request to complete.

Module usage
------------
.. _impstats.parameter.module.push-timeout-ms-usage:

.. code-block:: rsyslog

   module(load="impstats"
          push.timeout.ms="2000")

See also
--------
See also :doc:`../../configuration/modules/impstats`.
