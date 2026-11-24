.. SPDX-License-Identifier: Apache-2.0

.. _prop-system-time-now-unixtimestamp:
.. _properties.system-time.now-unixtimestamp:

.. meta::
   :description: Unix timestamp system property and an illustrative rate limit.
   :keywords: rsyslog, Unix timestamp, system property, rate limit

$now-unixtimestamp
==================

.. index::
   single: properties; $now-unixtimestamp
   single: $now-unixtimestamp

.. summary-start

Exposes the current time as a monotonically increasing Unix timestamp.

.. summary-end

This property belongs to the **Time-Related System Properties** group.

:Name: $now-unixtimestamp
:Category: Time-Related System Properties
:Type: integer

Description
-----------
The current time as a unix timestamp (seconds since epoch). This actually is a
monotonically increasing counter and as such can also be used for any other use
cases that require such counters.

Usage
-----
.. _properties.system-time.now-unixtimestamp-usage:

.. code-block:: rsyslog

   template(name="example" type="list") {
       property(name="$now-unixtimestamp")
   }

Additional example:

.. code-block:: none

   # Get Unix timestamp of current message
   set $.tnow = $$now-unixtimestamp;

   # Allow up to 5 matching messages in each counter window
   if ($!severity == 6 and $!facility == 17) then {
     if (($.tnow - $/trate) > 60) then {
       # More than 60 seconds since the window start: reset the counter
       set $/trate = $.tnow;
       set $/ratecount = 0;
     }
     if ($/ratecount >= 5) then {
       # discard message
       stop
     } else {
       set $/ratecount = $/ratecount + 1;
     }
   }

This example assumes messages have ``$!severity`` and ``$!facility`` fields.
It resets the counter when a matching message arrives more than 60 seconds
after the window start. It does not enforce a rolling 60-second limit. The
updates to the global variables are separate operations, so process these
messages with a single worker to preserve the per-window count.

Notes
~~~~~
.. note::
   By definition, there is no "UTC equivalent" of the ``$now-unixtimestamp``
   property.

- Properties that include hyphens require the double dollar form (``$$``) when
  used in expressions so the parser does not treat the hyphen as subtraction.

See also
--------
See :doc:`../../configuration/properties` for the category overview.
