*************
format_time()
*************

Purpose
=======

.. note::

   This is EXPERIMENTAL code - it may be removed or altered in
   later versions than 8.30.0. Please watch the ChangeLog closely for
   updates.

Converts a UNIX timestamp to a formatted RFC 3164 or RFC 3339 date/time string.
The first parameter is expected to be an integer value representing the number of
seconds since 1970-01-01T00:00:0Z (UNIX epoch). The second parameter can be one of
``"date-rfc3164"`` or ``"date-rfc3339"``. The output is a string containing
the formatted date/time. Date/time strings are expressed in **UTC** (no time zone
conversion is provided).

.. note::

   If the input to the function is NOT a proper UNIX timestamp, a string
   containing the original value of the parameter will be returned instead of a
   formatted date/time string.


Example
=======

RFC 3164 timestamp
------------------

In the following example the integer representing a UNIX timestamp is
formatted to a rfc-3164 date/time string.

.. code-block:: none

   format_time(1507165811, "date-rfc3164")

produces

.. code-block:: none

   Oct  5 01:10:11


Wrong input
-----------

In the following example a wrong parameter is given which can't be
formatted and so it is returned unchanged.

.. code-block:: none

   format_time("foo", "date-rfc3164")

produces

.. code-block:: none

   foo


