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

.. _ratelimit_persource:

perSource
^^^^^^^^^

.. csv-table::
   :header: "type", "required", "default"
   :widths: 20, 10, 20

   "boolean", "no", "off"

Enable per-source rate limiting using an external YAML policy.

.. _ratelimit_persourcepolicy:

perSourcePolicy
^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "required", "default"
   :widths: 20, 10, 20

   "string", "no", "none"

Path to the YAML file that defines per-source limits. Required when ``perSource`` is ``on``.
The YAML file must define a ``default`` block with ``max`` and ``window`` values
and may optionally include ``overrides`` keyed by exact sender values.

.. code-block:: yaml

   default:
     max: 1000
     window: 10s
   overrides:
     - key: "db01.corp.local"
       max: 5000
       window: 10s

.. _ratelimit_persourcekeytpl:

perSourceKeyTpl
^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "required", "default"
   :widths: 20, 10, 20

   "string", "no", "RSYSLOG_PerSourceKey"

Template that computes the per-source key. The default template is equivalent to
``%hostname%``.

.. _ratelimit_persourcemaxstates:

perSourceMaxStates
^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "required", "default"
   :widths: 20, 10, 20

   "integer", "no", "10000"

Upper bound on the number of tracked sender keys for per-source limits. When the cap is reached,
least-recently-used sender state is evicted.

.. _ratelimit_persourcetopn:

perSourceTopN
^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "required", "default"
   :widths: 20, 10, 20

   "integer", "no", "10"

Number of per-source drop counters to expose in statistics output (top-N by drops).

Example
-------

.. code-block:: rsyslog

   # Define a strict rate limit for public facing ports
   ratelimit(name="strict" interval="1" burst="50")

   # Define per-source policy for TCP inputs
   ratelimit(name="per_source"
             perSource="on"
             perSourcePolicy="/etc/rsyslog/imtcp-ratelimits.yaml"
             perSourceKeyTpl="PerSourceKey")

   # Apply it to a TCP listener
   input(type="imtcp" port="10514" rateLimit.Name="strict")

   # Apply it to a Plain TCP listener
   input(type="imptcp" port="10515" rateLimit.Name="strict")

   # Apply per-source limits to a TCP listener
   input(type="imtcp" port="10516" rateLimit.Name="per_source")

Per-source key examples
-----------------------

.. code-block:: rsyslog

   # Key by IP address
   template(name="PerSourceIP" type="string" string="%fromhost-ip%")
   ratelimit(name="per_source_ip"
             perSource="on"
             perSourcePolicy="/etc/rsyslog/imtcp-ratelimits.yaml"
             perSourceKeyTpl="PerSourceIP")
   input(type="imtcp" port="514" rateLimit.Name="per_source_ip")

   # Key by hostname (default)
   template(name="PerSourceHost" type="string" string="%hostname%")
   ratelimit(name="per_source_host"
             perSource="on"
             perSourcePolicy="/etc/rsyslog/imtcp-ratelimits.yaml"
             perSourceKeyTpl="PerSourceHost")
   input(type="imtcp" port="514" rateLimit.Name="per_source_host")
