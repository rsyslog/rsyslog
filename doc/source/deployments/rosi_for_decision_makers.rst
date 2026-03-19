.. _rosi-for-decision-makers:

ROSI for Decision-Makers
========================

.. meta::
   :description: Strategic ROSI overview for decision-makers focused on operational sustainability, lock-in risk, and phased adoption.
   :keywords: rsyslog, ROSI, operations strategy, vendor lock-in, sustainability, observability

.. summary-start

ROSI provides a concrete, deployable observability baseline while preserving
freedom of choice. It is designed to reduce lock-in risk and support
sustainable operations as requirements change.

.. summary-end

This page is for engineering managers, architects, and technical leaders who
need to balance delivery speed with long-term platform flexibility.

What ROSI Delivers Now
----------------------

ROSI is not a roadmap-only idea. The current ROSI Collector stack is deployable
today and provides:

- Centralized collection with rsyslog
- Log storage and search in a production-ready baseline
- Dashboards and metrics for operational visibility
- Turnkey setup that reduces early integration effort
- Container-based deployment model for practical operations today

See :doc:`rosi_collector/index` for implementation details.

Current State and Broader Direction
-----------------------------------

ROSI Collector is the main visible and ready-to-run ROSI artifact today. It is
the clearest starting point for teams that want immediate operational value.

The broader ROSI direction is larger than that single deployment profile. It
includes replaceable backend choices, mixed-component topologies, and
Windows-side collection paths. Many of these implementation patterns have
existed around rsyslog for a long time already. ROSI formalizes that operating
model, provides clearer guidance, and adds progressively more turnkey artifacts.
Those artifacts are not yet all equally mature, which is why ROSI Collector is
the clearest starting point today.

That broader space can include destinations such as Elasticsearch, OpenSearch,
Splunk, VictoriaLogs, Kafka, and HTTP-based services, plus Windows-side
components such as `rsyslog Windows Agent <https://www.rsyslog.com/windows-agent/>`_,
`WinSyslog <https://www.winsyslog.com/>`_, `EventReporter <https://www.eventreporter.com/>`_,
and `MonitorWare Agent <https://www.monitorware.com/>`_.

Why This Matters Strategically
------------------------------

Most observability programs fail in one of two ways:

- They remain fragmented and expensive to operate
- They become tightly coupled to a single vendor and hard to evolve

ROSI addresses both by combining a practical default with an explicit
freedom-of-choice architecture stance.

It also adds an efficiency posture that is increasingly important for both
FinOps and Green IT programs.

Decision Criteria ROSI Supports
-------------------------------

ROSI is a fit when you need to optimize for:

- **Time to value**: a concrete stack that can be deployed quickly
- **Operational sustainability**: simpler initial rollout without closing future options
- **Risk control**: lower long-term lock-in risk through replaceable components
- **Growth**: a path from small deployments to broader platform standardization
- **Efficiency**: better compute economics through efficient ingestion and routing

In many environments, placing rsyslog at the edge helps control central ingest
volume and therefore backend cost.

Risk Posture and Messaging
--------------------------

ROSI emphasizes architectural properties instead of vendor positioning:

- Open interfaces and standards-oriented integration
- Replaceable stack components
- Incremental migration rather than forced rewrites

This keeps decisions grounded in sustainable operations and freedom of choice.

Scale Profile
-------------

ROSI is often an excellent fit for small to medium installations where
efficiency is highly valued. It can also scale down effectively for homelab
use, making it suitable for both evaluation and lightweight production use.

Future evolution toward Kubernetes can build on the current container-based
foundation without invalidating early adoption.

Recommended Next Reading
------------------------

- :doc:`rosi_for_platform_teams` for architecture and adoption patterns
- :doc:`../getting_started/rosi_for_beginners` for onboarding context
- :doc:`rosi_collector/index` for concrete deployment details
