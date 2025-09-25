.. _prop-system-time-now-unixtimestamp:
.. _properties.system-time.now-unixtimestamp:

$now-unixtimestamp
==================

.. index::
   single: properties; $now-unixtimestamp
   single: $now-unixtimestamp

.. summary-start

Returns the current time as a monotonically increasing Unix timestamp.

.. summary-end

This property belongs to the **Time-Related System Properties** group.

:Name: $now-unixtimestamp
:Category: Time-Related System Properties
:Type: integer

Description
-----------
The current time as a unix timestamp (seconds since epoch). This actually is a
monotonically increasing counter and as such can also be used for any other use
cases that require such counters. This is an example of how to use it for
rate-limiting::

    # Get Unix timestamp of current message
    set $.tnow = $$now-unixtimestamp

    # Rate limit info to 5 every 60 seconds
    if ($!severity == 6 and $!facility == 17) then {
      if (($.tnow - $/trate) > 60) then {
        # 5 seconds window expired, allow more messages
        set $/trate = $.tnow;
        set $/ratecount = 0;
      }
      if ($/ratecount > 5) then {
        # discard message
        stop
      } else {
        set $/ratecount = $/ratecount + 1;
      }
    }

NOTE: by definition, there is no "UTC equivalent" of the $now-unixtimestamp
property.

Usage
-----
.. _properties.system-time.now-unixtimestamp-usage:

.. code-block:: rsyslog

   template(name="show-now-unixtimestamp" type="string" string="%$now-unixtimestamp%")

Notes
~~~~~
Because this property is monotonic, it can double as a counter for rate-limited
processing.

See also
--------
See :doc:`../../rainerscript/properties` for the category overview.
