.. _param-mmexternal-forcesingleinstance:
.. _mmexternal.parameter.input.forcesingleinstance:

forceSingleInstance
===================

.. index::
   single: mmexternal; forceSingleInstance
   single: forceSingleInstance

.. summary-start

Enforces that only a single instance of the external message modification
plugin runs.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/mmexternal`.

:Name: forceSingleInstance
:Scope: input
:Type: boolean
:Default: off
:Required?: no
:Introduced: 8.3.0

Description
-----------
This is an expert parameter, just like the equivalent *omprog* parameter. See
the message modification plugin's documentation if it is needed.

Input usage
-----------
.. _mmexternal.parameter.input.forcesingleinstance-usage:

.. code-block:: rsyslog

   module(load="mmexternal")

   action(
       type="mmexternal"
       binary="/path/to/mmexternal.py"
       forceSingleInstance="on"
   )

See also
--------
See also :doc:`../../configuration/modules/mmexternal`.
