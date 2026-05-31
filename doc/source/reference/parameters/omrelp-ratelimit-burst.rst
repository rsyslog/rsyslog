.. _param-omrelp-ratelimit-burst:

RateLimit.Burst
^^^^^^^^^^^^^^^

.. summary-start
Sets how many messages one omrelp action may forward per rate-limit interval.
.. summary-end

.. csv-table::
   :header: "type", "default", "max", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "200", "(2^32)-1", "no", "none"

Specifies the number of messages an omrelp action may forward during one
rate-limit interval.
