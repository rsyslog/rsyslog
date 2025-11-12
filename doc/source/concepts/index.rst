.. _concepts-index:

========
Concepts
========

.. meta::
   :description: Core rsyslog concepts including the log pipeline, queues, and message processing.
   :keywords: rsyslog, concepts, architecture, log pipeline, queues, message parser, multi-ruleset, janitor, network drivers

.. summary-start

Explains rsyslog’s core architectural concepts — from the **log pipeline**
(inputs → rulesets → actions) and its supporting queues, to components like
the janitor, message parser, and network stream drivers.

.. summary-end


Overview
--------
This chapter describes the **core building blocks** of rsyslog — the objects and
mechanisms that make up its architecture.  These topics are useful when you
want to understand how rsyslog processes events internally, how queues and
workers interact, or how different rulesets isolate workloads.

Each concept page provides a focused explanation and points to configuration
options that control the behavior of that subsystem.

.. note::

   The pages in this chapter are primarily **conceptual**.  For configuration
   syntax and examples, see the corresponding pages under
   :doc:`../configuration/index`.

Core concepts
-------------
Below is an overview of the conceptual topics included in this chapter.

.. toctree::
   :maxdepth: 2

   log_pipeline/index
   queues
   janitor
   messageparser
   multi_ruleset
   netstrm_drvr


How to use this section
-----------------------
- Start with :doc:`log_pipeline/index` to learn how rsyslog structures
  its event flow — this is the foundation for all other components.
- Continue with :doc:`queues` to understand how rsyslog handles buffering,
  reliability, and concurrency between pipeline stages.
- Review :doc:`messageparser` for insight into how raw inputs are parsed into
  structured properties.
- Explore :doc:`multi_ruleset` to learn about isolating or chaining processing
  logic.
- The :doc:`janitor` and :doc:`netstrm_drvr` pages explain how background
  maintenance and low-level network handling work.

Together these topics give you a complete conceptual understanding of how
rsyslog’s internal engine moves, filters, and stores log data.

