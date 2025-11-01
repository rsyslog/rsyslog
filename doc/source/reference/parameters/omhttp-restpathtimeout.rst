.. _param-omhttp-restpathtimeout:
.. _omhttp.parameter.module.restpathtimeout:

restpathtimeout
===============

.. index::
   single: omhttp; restpathtimeout
   single: restpathtimeout

.. summary-start

Specifies how long omhttp waits for a dynamic REST path template to resolve.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omhttp`.

:Name: restpathtimeout
:Scope: module
:Type: integer
:Default: module=none
:Required?: no
:Introduced: Not specified

Description
-----------
Timeout value for the configured :ref:`param-omhttp-restpath`.

Module usage
------------
.. _omhttp.parameter.module.restpathtimeout-usage:

.. code-block:: rsyslog

   module(load="omhttp")

   action(
       type="omhttp"
       restPathTimeout="2000"
   )

See also
--------
See also :doc:`../../configuration/modules/omhttp`.
