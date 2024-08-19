timezone
========

.. index:: ! timezone
.. _cfgobj_input:

The ``timezone`` object, as its name suggests, describes timezones.
Currently, they are used by message parser modules to interpret
timestamps that contain timezone information via a timezone string
(but not an offset, e.g. "CET" but not "-01:00"). The object describes
an UTC offset for a given timezone ID.

Each timestamp object adds the zone definition to a global table
with timezone information. Duplicate IDs are forbidden, but the
same offset may be used with multiple IDs.

As with other configuration objects, parameters for this
object are case-insensitive.


Parameters
----------

.. function::  id <name-string>

   *Mandatory*

   This identifies the timezone. Note that this id must match the zone
   name as reported within the timestamps. Different devices and vendors
   use different, often non-standard, names and so it is important to use
   the actual ids that messages contain. For multiple devices, this may
   mean that you may need to include multiple definitions, each one with a
   different id, for the same time zone. For example, it is seen that
   some devices report "CEST" for central European daylight savings time
   while others report "METDST" for it.

.. function::  offset <[+/-]><hh>:<mm>

   *Mandatory*

   This defines the timezone offset over UTC. It must always be 6 characters
   and start with a "+" (east of UTC) or "-" (west uf UTC) followed by a
   two-digit hour offset, a colon and a two-digit minute offset. Hour offsets
   can be in the range from zero to twelve, minute offsets in the range from
   zero to 59. Any other format is invalid.

Sample
------
The following sample defines UTC time. From rsyslog PoV, it doesn't
matter if a plus or minus offset prefix is used. For consistency,
plus is suggested.

::

  timezone(id="UTC" offset="+00:00")

The next sample defines some common timezones:

::

  timezone(id="CET" offset="+01:00")
  timezone(id="CEST" offset="+02:00")
  timezone(id="METDST" offset="+02:00") # duplicate to support different formats
  timezone(id="EST" offset="-05:00")
  timezone(id="EDT" offset="-04:00")
  timezone(id="PST" offset="-08:00")
  timezone(id="PDT" offset="-07:00")

