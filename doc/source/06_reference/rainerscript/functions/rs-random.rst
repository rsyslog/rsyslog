********
random()
********

Purpose
=======

random(max)

Generates a random number between 0 and the number specified, though
the maximum value supported is platform specific.

- If a number is not specified then 0 is returned.
- If 0 is provided as the maximum value, then 0 is returned.
- If the specified value is greater than the maximum supported
  for the current platform, then rsyslog will log this in
  the debug output and use the maximum value supported instead.

While the original intent of this function was for load balancing, it
is generic enough to be used for other purposes as well.

.. warning::
   The random number must not be assumed to be crypto-grade.

.. versionadded:: 8.12.0


Example
=======

In the following example a random number between 0 and 123456 is generated.

.. code-block:: none

   random(123456)

