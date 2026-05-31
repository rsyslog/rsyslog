.. _param-omrelp-ratelimit-interval:

RateLimit.Interval
^^^^^^^^^^^^^^^^^^

.. summary-start

Sets the omrelp action-local rate-limit interval in seconds. ``0`` disables
rate limiting.
.. summary-end

.. csv-table::
   :header: "type", "default", "max", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "0", "", "no", "none"

Specifies the rate-limiting interval in seconds for this omrelp action.
The default value is ``0``, which disables rate limiting.
