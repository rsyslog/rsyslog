*********
is_time()
*********

Purpose
=======

is_time(timestamp)
is_time(timestamp, format_str)

Checks the given timestamp to see if it is a valid date/time string (RFC 3164,
or RFC 3339), or a UNIX timestamp.

This function returns ``1`` for valid date/time strings and UNIX timestamps,
``0`` otherwise. Additionally, if the input cannot be parsed, or there is
an error, ``script_error()`` will be set to error state.

The ``format_str`` parameter is optional, and can be one of ``"date-rfc3164"``,
``"date-rfc3339"`` or ``"date-unix"``. If this parameter is specified, the
function will only succeed if the input matches that format. If omitted, the
function will compare the input to all of the known formats (indicated above)
to see if one of them matches.

.. note::

   This function does not support unusual RFC 3164 dates/times that
   contain year or time zone information.


Example
=======

Only timestamp is given
-----------------------

The following example shows the output when a valid timestamp is given.

.. code-block:: none

   is_time("Oct  5 01:10:11")
   is_time("2017-10-05T01:10:11+04:00")
   is_time(1507165811)

all produce

.. code-block:: none

   1


Timestamp and Format given
--------------------------

The following example shows the output when a valid timestamp is given but
the format does not match.

.. code-block:: none

   is_time("2017-10-05T01:10:11+04:00", "date-unix")

all produce

.. code-block:: none

  0 

