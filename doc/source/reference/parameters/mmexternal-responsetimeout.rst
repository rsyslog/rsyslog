.. _param-mmexternal-responsetimeout:
.. _mmexternal.parameter.input.responsetimeout:

responseTimeout
===============

.. index::
   single: mmexternal; responseTimeout
   single: responseTimeout

.. summary-start

Bounds how long mmexternal waits for a JSON response from the external plugin.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/mmexternal`.

:Name: responseTimeout
:Scope: input
:Type: integer
:Default: 0
:Required?: no
:Introduced: 8.2606.0

Description
-----------
This parameter sets the maximum time, in milliseconds, that ``mmexternal``
waits for the external program to return a complete JSON response line.
The default value ``0`` keeps the historical behavior and waits without a
module-level timeout.

When the timeout expires, ``mmexternal`` logs a warning, terminates and
restarts the external program, and continues processing. The current message
is not retried through the timed-out helper.

The timeout applies only when :ref:`param-mmexternal-interface-output` is
``"json"``. It has no effect when ``interface.output="none"``.

Input usage
-----------
.. _param-mmexternal-input-responsetimeout:
.. _mmexternal.parameter.input.responsetimeout-usage:

.. code-block:: rsyslog

   module(load="mmexternal")

   action(
       type="mmexternal"
       binary="/path/to/mmexternal.py"
       responseTimeout="500"
   )

See also
--------
See also :doc:`../../configuration/modules/mmexternal`.
