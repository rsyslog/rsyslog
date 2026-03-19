.. _rosi-for-platform-teams:

ROSI for Platform Teams
=======================

.. meta::
   :description: ROSI guidance for platform engineers focused on architecture, adoption, and migration without lock-in.
   :keywords: rsyslog, ROSI, platform engineering, architecture, migration, vendor lock-in

.. summary-start

ROSI gives platform teams a practical way to standardize log collection while
keeping backend and integration choices open. It is designed for incremental
adoption, not all-at-once migration.

.. summary-end

This guide is for teams operating shared logging infrastructure across multiple
services, clusters, or business units.

Design Intent
-------------

ROSI is built to solve a common platform problem: teams need a deployable
default stack now, but they also need an architecture that can evolve later.

For platform teams, this means:

- A clear baseline with :doc:`rosi_collector/index`
- rsyslog-centered collection and routing as a stable control point
- Freedom to adapt downstream storage and analytics over time

Where ROSI Is Today
-------------------

The main packaged ROSI artifact today is :doc:`rosi_collector/index`. That is
the recommended operational baseline because it is the most complete and
ready-to-run profile.

At the same time, ROSI is intentionally broader than ROSI Collector alone.
Platform teams can already combine rsyslog-centered ingestion with additional
destinations and side components. That is not new in itself: many teams have
been doing this with rsyslog for years. What ROSI adds is a clearer
formalization of those patterns, stronger guidance, and a growing set of
artifacts that reduce setup effort. Those artifacts are not yet all equally
turnkey, which is why ROSI Collector remains the main packaged baseline today.

It also means optimizing for efficiency, not just feature count:

- Lower operational overhead for small and medium environments
- Better resource usage through early routing/filtering at ingestion
- Practical operation in constrained environments, including homelabs

Architecture Stance
-------------------

ROSI intentionally separates concerns:

- **Collection and transport**: handled by rsyslog
- **Storage and query**: chosen per operational need
- **Visualization and alerting**: chosen per team workflow

This reduces coupling and keeps migration scope manageable.

Incremental Adoption Pattern
----------------------------

A practical rollout path:

1. Start with ROSI Collector for immediate operational value.
2. Standardize client forwarding and labeling conventions.
3. Introduce additional destinations where needed (parallel or staged).
4. Migrate dashboards and alerting workflows in controlled phases.

The key principle is incremental change with stable ingestion.

Container-First, Kubernetes-Next
--------------------------------

The current ROSI stack is container-based and intentionally straightforward to
operate. Kubernetes is a next target, but not a prerequisite for value today.

This container-first approach aligns with efficiency goals for teams that want
predictable operations without immediate orchestration complexity.

Integration Paths
-----------------

rsyslog already supports multiple output paths that help avoid hard coupling:

- :doc:`../configuration/modules/omhttp` for HTTP targets
- :doc:`../configuration/modules/omelasticsearch` for Elasticsearch and OpenSearch
- :doc:`../configuration/modules/omkafka` for Kafka-based pipelines
- :doc:`../configuration/modules/omfwd` for syslog forwarding

This enables practical designs such as dual-write transitions, phased
cutovers, and domain-specific routing.

In ROSI terms, this means the current collector profile can coexist with
additional destinations such as Splunk, VictoriaLogs, or other HTTP-accessible
backends when platform requirements call for that.

The broader ROSI picture can also include Windows-side components such as
`rsyslog Windows Agent <https://www.rsyslog.com/windows-agent/>`_,
`WinSyslog <https://www.winsyslog.com/>`_, `EventReporter <https://www.eventreporter.com/>`_,
and `MonitorWare Agent <https://www.monitorware.com/>`_ where those products
fit existing operational models.

ROSI therefore helps in three ways at once: it preserves architectural freedom,
it gives clearer guidance on how to use that freedom well, and it gradually
adds more ready-made artifacts around proven rsyslog-based patterns.

Using rsyslog on the edge can reduce central processing cost by shaping data
before it reaches backends, which supports both FinOps and Green IT targets.

Governance Recommendations
--------------------------

To keep freedom of choice real in daily operations:

- Treat destination-specific logic as replaceable policy
- Keep ingestion contracts stable (fields, labels, transport expectations)
- Document migration playbooks before they are urgently needed
- Prefer reversible rollout steps over one-way platform changes

See Also
--------

- :doc:`rosi_collector/index` for concrete deployment details
- :doc:`rosi_for_decision_makers` for sustainability and risk framing
- :doc:`../getting_started/rosi_for_beginners` for newcomer context
