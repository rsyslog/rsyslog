.. _param-omhttp-restpathtimeout:
.. _omhttp.parameter.input.restpathtimeout:

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
:Scope: input
:Type: integer
:Default: input=none
:Required?: no
:Introduced: Not specified

Description
-----------
Timeout value for the configured :ref:`param-omhttp-restpath`.

Input usage
-----------
.. _omhttp.parameter.input.restpathtimeout-usage:

.. code-block:: rsyslog

   module(load="omhttp")

   action(
       type="omhttp"
       restPathTimeout="2000"
   )

See also
--------
See also :doc:`../../configuration/modules/omhttp`.
