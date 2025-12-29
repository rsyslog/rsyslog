.. _ratelimit:

ratelimit Object
================

.. versionadded:: 8.2602.0

The ``ratelimit`` object allows defining named rate limit policies that can be reused across multiple inputs.
This is particularly useful for applying a consistent policy to a group of listeners or for managing rate limits centrally.

Parameters
----------

.. _ratelimit_name:

name
^^^^

.. csv-table::
   :header: "type", "required", "default"
   :widths: 20, 10, 20

   "string", "yes", "none"

The name of the rate limit policy. This name is used to reference the policy from input modules (e.g., via ``RateLimit.Name="policyName"``).

.. _ratelimit_interval:

interval
^^^^^^^^

.. csv-table::
   :header: "type", "required", "default"
   :widths: 20, 10, 20

   "integer", "no", "0"

The interval (in seconds) for the rate limit. Messages exceeding the ``burst`` limit within this interval are dropped.
A value of 0 disables rate limiting.

.. _ratelimit_burst:

burst
^^^^^

.. csv-table::
   :header: "type", "required", "default"
   :widths: 20, 10, 20

   "integer", "no", "10000"

The maximum number of messages allowed within the ``interval``.

Example
-------

.. code-block:: rsyslog

   # Define a strict rate limit for public facing ports
   ratelimit(name="strict" interval="1" burst="50")

   # Apply it to a TCP listener
   input(type="imtcp" port="10514" rateLimit.Name="strict")

   # Apply it to a Plain TCP listener
   input(type="imptcp" port="10515" rateLimit.Name="strict")
