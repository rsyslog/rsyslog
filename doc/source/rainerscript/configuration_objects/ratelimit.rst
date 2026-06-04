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

.. _ratelimit_severity:

severity
^^^^^^^^

.. csv-table::
   :header: "type", "required", "default"
   :widths: 20, 10, 20

   "severity", "no", "0"

Only messages with a syslog severity numerically greater than or equal to this
threshold are subject to rate limiting. Lower numeric severities remain
unlimited.

This is useful when a shared policy should throttle verbose traffic such as
``info`` or ``debug`` while leaving more urgent messages untouched. Modules that
derive severity from input metadata, such as ``imjournal``, can use the same
shared policy without a module-specific severity knob.

.. _ratelimit_policywatch:

policyWatch
^^^^^^^^^^^

.. csv-table::
   :header: "type", "required", "default"
   :widths: 20, 10, 20

   "boolean", "no", "off"

Enable automatic reload of configured external policy files when they change.
When watch support is available, rsyslog monitors the configured ``policy`` and
legacy ``perSourcePolicy`` files and reloads them after the debounce interval. When
watch support is unavailable in the current build or runtime environment,
rsyslog logs a warning and continues with HUP-only reload behavior.

.. _ratelimit_policywatchdebounce:

policyWatchDebounce
^^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "required", "default"
   :widths: 20, 10, 20

   "time interval", "no", "5s"

Quiet period applied to ``policyWatch`` reloads. Each new file event resets the
timer, so rapid updates are coalesced into one reload. Supported suffixes are
``ms``, ``s``, ``m``, and ``h``. Bare numbers are interpreted as seconds.

.. _ratelimit_persource:

perSource
^^^^^^^^^

.. csv-table::
   :header: "type", "required", "default"
   :widths: 20, 10, 20

   "boolean", "no", "off"

Enable per-source rate limiting using an external YAML policy.

For new configurations, prefer defining the complete per-source policy inside
the main ``policy`` YAML file. The inline ``perSource`` parameter remains
available for split legacy configurations that use ``perSourcePolicy``.

.. _ratelimit_policy:

policy
^^^^^^

.. csv-table::
   :header: "type", "required", "default"
   :widths: 20, 10, 20

   "string", "no", "none"

Path to a YAML file that defines rate limit settings. The file can contain
global settings and, for new configurations, a nested ``perSource`` section:

.. code-block:: yaml

   interval: 1
   burst: 50
   severity: info

   perSource:
     enabled: true
     keyTemplate: "PerSourceIP"
     maxStates: 10000
     topN: 10
     default:
       max: 1000
       window: 10s
     overrides:
       - key: "db01.corp.local"
         max: 5000
         window: 10s

``perSource.keyTemplate`` is an rsyslog template name. On HUP or watched file
reload, valid policy changes replace the previous global limits, per-source
defaults and overrides, key template, ``maxStates``, and ``topN``. Invalid
reloads keep the previous valid policy active.

Do not mix a ``policy`` YAML file that contains ``perSource`` with legacy
per-source parameters on the same ``ratelimit()`` object, such as
``perSource``, ``perSourcePolicy``, ``perSourceKeyTpl``,
``perSourceMaxStates``, or ``perSourceTopN``.

.. _ratelimit_persourcepolicy:

perSourcePolicy
^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "required", "default"
   :widths: 20, 10, 20

   "string", "no", "none"

Compatibility path to a YAML file that defines per-source limits separately
from the main ``policy`` file. Required when the inline ``perSource`` parameter
is ``on`` and the main ``policy`` file does not contain a ``perSource`` section.
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

Template that computes the per-source key for split legacy configurations. The
default template is equivalent to ``%hostname%``. In the canonical single-file
``policy`` YAML, use ``perSource.keyTemplate`` instead.

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

   # Rate-limit only lower-priority journal traffic
   ratelimit(name="journal_lowprio"
             interval="600"
             burst="20000"
             severity="info")

   # Define a watched YAML policy for automatic reload
   ratelimit(name="watched"
             policy="/etc/rsyslog/ratelimit.yaml"
             policyWatch="on"
             policyWatchDebounce="500ms")

   # Define per-source policy for TCP inputs
   template(name="PerSourceIP" type="string" string="%fromhost-ip%")
   ratelimit(name="per_source"
             policy="/etc/rsyslog/imtcp-ratelimits.yaml")

   # Apply it to a TCP listener
   input(type="imtcp" port="10514" rateLimit.Name="strict")

   # Apply it to a Plain TCP listener
   input(type="imptcp" port="10515" rateLimit.Name="strict")

   # Apply per-source limits to a TCP listener
   input(type="imtcp" port="10516" rateLimit.Name="per_source")

   # Apply a severity-aware shared policy to the journal reader
   module(load="imjournal" rateLimit.Name="journal_lowprio")

Per-source key examples
-----------------------

The policy file selects the template through ``perSource.keyTemplate``; defining
the template alone is not enough.

.. code-block:: yaml

   # /etc/rsyslog/imtcp-ratelimits-ip.yaml
   interval: 1
   burst: 100
   perSource:
     enabled: true
     keyTemplate: "PerSourceIP"
     default:
       max: 10
       window: 1s

.. code-block:: rsyslog

   # Key by IP address
   template(name="PerSourceIP" type="string" string="%fromhost-ip%")
   ratelimit(name="per_source_ip"
             policy="/etc/rsyslog/imtcp-ratelimits-ip.yaml")
   input(type="imtcp" port="514" rateLimit.Name="per_source_ip")

.. code-block:: yaml

   # /etc/rsyslog/imtcp-ratelimits-host.yaml
   interval: 1
   burst: 100
   perSource:
     enabled: true
     keyTemplate: "PerSourceHost"
     default:
       max: 10
       window: 1s

.. code-block:: rsyslog

   # Key by hostname (default)
   template(name="PerSourceHost" type="string" string="%hostname%")
   ratelimit(name="per_source_host"
             policy="/etc/rsyslog/imtcp-ratelimits-host.yaml")
   input(type="imtcp" port="514" rateLimit.Name="per_source_host")

Input modules only reference the named ratelimit object with
``rateLimit.Name``. Per-source key evaluation and enforcement are handled by the
shared ratelimit object so modules inherit new ratelimit features without
module-specific API calls.

Per-source limit hits use the same discard semantics as global ratelimit hits:
the message is consumed by the ratelimiter and is not counted as successfully
submitted by the input module. Transport callbacks that must acknowledge a
discarded message, such as RELP, handle that acknowledgement in the module after
the ratelimiter has reported the discard.
