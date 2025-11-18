.. _about-history:

.. meta::
   :description: Reverse-chronological history of rsyslog, from the AI First era back to its
                 early releases after the sysklogd fork.
   :keywords: rsyslog, history, syslog, sysklogd, RainerScript, logging, observability, AI First

.. summary-start

A reverse-chronological history of rsyslog covering its evolution from the AI First era back to
its early releases following the 2004 fork from sysklogd.

.. summary-end


History of rsyslog
==================

rsyslog builds on more than four decades of syslog evolution. This page covers the project’s
history from the present day back to the period shortly after its creation in 2004. For the
earlier syslog heritage (1983–2003), including the foundations established by BSD syslogd, see
:doc:`origins`.

This timeline is presented in strict **reverse-chronological** order.


2025 – AI First Strategy
------------------------

In 2025, rsyslog adopted a formal **AI First** strategy to modernize development and
documentation workflows:

- AI-assisted documentation generation, restructuring, and drift monitoring.
- Faster contributor experiences through intelligent pre-checks and triage tools.
- A redesigned Beginner’s Guide and improved documentation structure (see
  :doc:`/getting_started/beginner_tutorials/index`).
- Early groundwork for responsible, human-reviewed AI-assisted log processing.

The strategy improves efficiency while preserving rsyslog’s established architecture.


2024 – First Production AI Artifacts and Intensive Human-in-the-Loop Work
-------------------------------------------------------------------------

In 2024, rsyslog delivered its first **production AI-assisted documentation updates**. These
improved clarity and structure and demonstrated that AI could support the project, though the
workflow required substantial human-in-the-loop review and was not yet suitable for large-scale
updates.

Internally, more advanced prototypes for code-oriented agents and contributor assistance were
developed. Their early maturity and cross-domain nature kept them non-public, but they informed
approaches later formalized in the 2025 AI First strategy.

The blog post *Documentation Improvement and AI* summarized the publicly visible results.


2019–2023 – Steady Progress and Early Internal AI Work
-------------------------------------------------------

Development continued steadily during these years despite reduced community activity caused by
the global pandemic. Work focused on module refinements, TLS enhancements, container deployment
guidance, and improvements to RainerScript.

From **late 2022**, early AI-assisted documentation and code-analysis experiments began
internally, laying the groundwork for the externally visible AI work of 2024.


2018 – Expansion into Observability Ecosystems
----------------------------------------------

By 2018, rsyslog had become a core component in broader observability and event-processing
ecosystems:

- Mature Elasticsearch and Kafka integrations (see :doc:`/configuration/modules/omkafka`).
- Better container deployment practices (see :doc:`/containers/index`).
- Growing use as a general event pipeline engine rather than only a syslog daemon.


2014 – High-Performance Enterprise Logging
------------------------------------------

Around 2014, rsyslog achieved major performance improvements:

- Throughput in the **hundreds of thousands to near one million messages per second** on
  commodity hardware.
- High-quality Elasticsearch and Kafka output modules (see
  :doc:`/configuration/modules/omelasticsearch`).
- Improvements in multi-threading, buffering, and queue design.

This period established rsyslog as a strong choice for high-volume enterprise logging.


2010 – Stabilized Modular Architecture
--------------------------------------

By 2010, rsyslog’s architecture had matured into the modular framework used today:

- Standard module families: **im\*** (inputs) and **om\*** (outputs)
  (see :doc:`/configuration/modules/index`).
- Structured logging with JSON templates.
- RainerScript-driven control flow (see :doc:`/rainerscript/index`).
- Clear separation of inputs, rulesets, queues, and actions (see :doc:`/concepts/queues`).

This period marks rsyslog’s evolution into a flexible, general-purpose log-processing engine.


2008 – Queue Engine and RainerScript
------------------------------------

2008 introduced two defining technologies.

### Multi-threaded queue engine
Released on January 31, 2008, enabling:

- scalable parallelism,
- disk-assisted buffering,
- flexible queue types.

### RainerScript
Introduced on February 28, 2008:

- expressive configuration language,
- typed evaluations,
- predictable rule processing,
- foundations for modern pipelines.

These innovations positioned rsyslog well ahead of contemporary syslog solutions.


2007 – Community Growth and Ecosystem Adoption
----------------------------------------------

Key milestones in 2007 included:

- Fedora 8 selecting rsyslog as its default syslog daemon.
- Red Hat’s involvement improving packaging and IPv6 support.
- BSD ports created for rsyslog and liblogging.
- Launch of the rsyslog wiki.
- Output subsystem refactoring that prepared for later plugin models.

These developments positioned rsyslog for the architectural breakthroughs of 2008–2010.


2005–2006 – Early Enhancements
------------------------------

During this period, rsyslog expanded steadily beyond sysklogd:

- improved templates and formatting,
- early TCP transport work,
- stronger timestamp handling,
- initial modularity concepts.

These incremental improvements built the foundation for the later 2.x and 3.x redesigns.


2004 – Project Launch
---------------------

rsyslog began in 2004 as a fork of sysklogd with the goal of maintaining syslog compatibility
while resolving key limitations:

- lack of reliable transport (TCP, later RFC 3195),
- limited formatting and timestamp precision,
- need for database output support,
- restricted extensibility.

These foundational improvements paved the way for the larger redesigns that followed.

