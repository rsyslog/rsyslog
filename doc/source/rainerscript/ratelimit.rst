ratelimit()
===========

The ``ratelimit()`` object defines a reusable rate-limiting
configuration. Once declared, instances can be referenced from inputs,
outputs, or other components via the :literal:`ratelimit.name` parameter.
This makes it trivial to keep rate limiting aligned across multiple
modules.

Example
-------

.. code-block:: none

   ratelimit(
       name="http_ingest",
       interval="60",
       burst="1200",
       severity="5"
   )

   input(type="imudp"
         port="514"
         ratelimit.name="http_ingest")

   action(type="omhttp"
          server="https://api.example"
          ratelimit.name="http_ingest")

Parameters
----------

``name``
   Unique identifier used by :literal:`ratelimit.name`. The object must be
   defined before it is referenced.

``interval``
   Time window, in seconds, that the ratelimiter covers. A value of
   ``0`` disables the limiter.

``burst``
   Number of messages that may pass through the limiter during each
   interval before throttling kicks in.

``severity`` *(optional)*
   Maximum severity level that is subject to rate limiting. Messages with
   a numerically lower severity (e.g. ``emerg``/``alert``) always pass the
   limiter. When omitted, all severities are rate limited.

Notes
-----

* Inline ``ratelimit.interval``/``ratelimit.burst`` settings remain
  supported for backward compatibility. When :literal:`ratelimit.name`
  is present, the inline parameters must be omitted.
* Multiple configuration blocks may reference the same ratelimit object.
  Each consumer keeps its own runtime counters while sharing the immutable
  configuration.
* The optional severity value only takes effect for components that
  support severity-based throttling (currently :doc:`../configuration/modules/imuxsock`).
