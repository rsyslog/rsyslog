************
parse_time()
************

Purpose
=======

parse_time(timestamp)

Converts an RFC 3164 or RFC 3339 formatted date/time string to a UNIX timestamp
(an integer value representing the number of seconds since the UNIX epoch:
1970-01-01T00:00:0Z).

If the input to the function is not a properly formatted RFC 3164 or RFC 3339
date/time string, or cannot be parsed, ``0`` is returned and ``script_error()``
will be set to error state.

.. note::

   This function does not support unusual RFC 3164 dates/times that
   contain year or time zone information.

.. note::

   Fractional seconds (if present) in RFC 3339 date/time strings will
   be discarded.


Example
=======

In the following example a timestamp is parsed into an integer.

.. code-block:: none

   parse_time("Oct 5 01:10:11") # Assumes the current year (2017, in this example)

produces

.. code-block:: none

   1507165811


