.. _getting-started-rosi:

ROSI for Beginners
==================

.. meta::
   :description: Beginner introduction to ROSI as an open, rsyslog-backed operations stack that reduces lock-in and complexity.
   :keywords: rsyslog, ROSI, vendor lock-in, observability, beginner, deployment

.. summary-start

ROSI (Rsyslog Operations Stack Initiative) gives beginners a concrete, deployable
stack while keeping freedom of choice. It is built to reduce vendor lock-in and
complexity, and to let teams evolve components over time.

.. summary-end

ROSI (Rsyslog Operations Stack Initiative) is the rsyslog approach to practical,
modern observability: start with a concrete stack you can deploy now, but do not
get trapped in one vendor or one fixed architecture.

Today, the main ready-to-run ROSI artifact is :doc:`../deployments/rosi_collector/index`.
That is the easiest way to get started, but ROSI itself is broader than that
single profile. The larger ROSI picture includes additional backend choices,
mixed-component deployments, and Windows-side collection options. Many of these
architectures have been built with rsyslog for a long time already; what ROSI
adds is a clearer formalization of that approach, better guidance, and a
growing set of artifacts that make adoption easier.

Why ROSI Exists
---------------

Many teams run into the same pain points:

- Logging stacks that are hard to deploy and operate
- rsyslog configurations that feel too complex for a first production rollout
- Tool choices that later create vendor lock-in

ROSI addresses these with a beginner-friendly default that is:

- **Easy to use**: a turnkey collector stack that works out of the box
- **Concrete**: not a future promise, but a deployable stack available now
- **Open and growing**: designed to expand over time as new stack profiles are added

ROSI at a Glance
----------------

.. raw:: html

   <pre class="mermaid mermaid-rosi-glance">
   flowchart LR
     P1["Complex setup"] --> R1["Turnkey ROSI stack"]
     P2["Vendor lock-in risk"] --> R2["Freedom of choice"]
     P3["High ingest and ops cost"] --> R3["Efficient rsyslog at ingestion"]

     R1 --> O1["Faster time to value"]
     R2 --> O2["Replaceable architecture"]
     R3 --> O3["Lower cost and energy use"]

     classDef pain fill:#fde2e2,stroke:#c0392b,color:#1f1f1f;
     classDef rosi fill:#e8f4fd,stroke:#2e86c1,color:#1f1f1f;
     classDef outcome fill:#e8f8f0,stroke:#1d8348,color:#1f1f1f;

     class P1,P2,P3 pain;
     class R1,R2,R3 rosi;
     class O1,O2,O3 outcome;
   </pre>
   <script src="../_static/vendor/mermaid/mermaid.min.js"></script>
   <script>
   window.addEventListener("load", function () {
     if (typeof mermaid === "undefined") return;
     mermaid.initialize({ startOnLoad: false });
     mermaid.run({ nodes: document.querySelectorAll(".mermaid-rosi-glance") });
   });
   </script>

Efficiency First (FinOps and Green IT)
--------------------------------------

ROSI also follows an efficiency-first mindset:

- Use resource-efficient components (including rsyslog at ingestion)
- Keep baseline operations practical for smaller and medium installations
- Support "right-sized" observability, including homelab-scale deployments

This helps align day-to-day operations with both **FinOps** goals (cost control)
and **Green IT** goals (lower compute footprint).

Freedom of Choice and Vendor Lock-In
------------------------------------

ROSI is intentionally built as a guard against vendor lock-in. The core idea is
simple: your logging pipeline should stay under your control.

In practice, that means:

- rsyslog remains a strong routing and processing layer at the center
- external components are selected for operational value, not lock-in
- you can adapt outputs and destinations as your requirements evolve

Today, rsyslog already supports multiple output targets and integration patterns,
including modules such as:

- :doc:`../configuration/modules/omhttp` for HTTP-based endpoints
- :doc:`../configuration/modules/omelasticsearch` for Elasticsearch and OpenSearch
- :doc:`../configuration/modules/omkafka` for Kafka
- :doc:`../configuration/modules/omfwd` for syslog forwarding

Common destination examples include Loki, Elasticsearch, OpenSearch, Kafka,
Splunk (HEC-style HTTP), VictoriaLogs, and cloud endpoints exposed over HTTP or
syslog.

ROSI can also include Windows-side collection components where they fit the
operational need, such as `rsyslog Windows Agent <https://www.rsyslog.com/windows-agent/>`_,
`WinSyslog <https://www.winsyslog.com/>`_, `EventReporter <https://www.eventreporter.com/>`_,
and `MonitorWare Agent <https://www.monitorware.com/>`_. These complement the
rsyslog-centered ingestion and routing model rather than replacing it.
Some of these components come from `Adiscon <https://www.adiscon.com/>`_, while
others are third-party tools that fit the same freedom-of-choice model.

In other words, ROSI does not invent freedom-of-choice logging around rsyslog.
Users have long connected rsyslog to destinations such as Elasticsearch,
Splunk, Kafka, syslog receivers, and HTTP-based services, sometimes in
parallel. ROSI formalizes that long-standing practice and provides clearer
guidance and more reusable artifacts around it.

As ROSI grows, additional pre-packaged stack profiles can be added without
changing this core principle: keep architectures replaceable.

This is the foundation for sustainable operations: freedom of choice instead of
forced rewrites.

Where rsyslog at the edge is used, teams can often reduce downstream ingest
volume and processing cost by routing, filtering, and shaping data before it
reaches central backends.

What ROSI Is and Is Not
-----------------------

ROSI **is**:

- A practical, rsyslog-backed observability approach
- A concrete starter stack (ROSI Collector) for centralized logs and metrics
- A formalization of long-standing rsyslog practices around replaceable
  architectures and parallel destinations
- A guidance layer that makes proven rsyslog-based patterns easier to choose
  and operate well
- A broader ecosystem direction that can incorporate additional backends and
  Windows-side components, with more turnkey artifacts being added over time
- A path that can grow with your infrastructure over time
- Container-based today, with Kubernetes-oriented evolution as a next target

ROSI **is not**:

- A closed product tied to a single vendor
- A claim that one backend is always right for every team
- A replacement for detailed implementation guides

Current Starter Stack
---------------------

The current ROSI starter deployment is :doc:`../deployments/rosi_collector/index`.
It combines rsyslog with Loki, Grafana, Prometheus, and Traefik to deliver a
production-ready baseline quickly.

This baseline solves immediate operational pain while preserving future options.
If you want a hands-on walkthrough, follow
:doc:`../tutorials/deploy_rsyslog_rosi_stack`.

What to Read Next
-----------------

- :doc:`../deployments/rosi_collector/index` for deployment and operations
- :doc:`../deployments/index` for deployment choices
- :doc:`next_steps` for the broader beginner path

For deeper follow-up:

- :doc:`../deployments/rosi_for_platform_teams` for platform architecture and adoption
- :doc:`../deployments/rosi_for_decision_makers` for sustainability and risk framing
