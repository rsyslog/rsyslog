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
This is an expert parameter. By default, ``mmexternal`` may start multiple
instances of the external program, for example when used with an action queue
with multiple worker threads. If you need to ensure that only a single
instance of the program is ever running, set this parameter to ``"on"``. This
is useful if the external program accesses a shared resource that does not
support concurrent access.

This parameter is equivalent to the
:ref:`param-omprog-forcesingleinstance` parameter.

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

Notes
-----
- The type was previously documented as ``binary``; this maps to ``boolean``.

See also
--------
See also :doc:`../../configuration/modules/mmexternal`.
