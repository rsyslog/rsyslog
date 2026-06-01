************
parse_time_localtz()
************

Purpose
=======

parse_time_localtz(timestamp)

Same function as "parse_time".

This function converts "timestamp" into epoch time but uses the Timezone offset of the Rsyslog server.

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

   parse_time_localtz("Oct 5 01:10:11") # Assumes the current year (2017, in this example)

produces

.. code-block:: none

   1507165811


