.. _beginner_tutorials:

Beginner Tutorials
##################

.. meta::
   :audience: beginner
   :tier: entry
   :keywords: rsyslog, tutorial, getting started, learning path, message pipeline

.. summary-start

A guided series of step-by-step tutorials for rsyslog beginners, built around
the message pipeline (input → ruleset → action).

.. summary-end

Welcome to the **Beginner Tutorials**, a curated learning path designed to help
you get from zero to a working rsyslog setup quickly and confidently.

Each tutorial is:

- **Goal-oriented** – solves a single, practical problem.

- **Runnable** – includes commented configuration examples.

- **Verifiable** – always shows how to test results (e.g., with ``logger``).

- **Resilient** – ends with an *If it's not working…* section.

We use the **message pipeline metaphor** throughout: rsyslog receives messages
through an *input*, processes them in a *ruleset* (filters, parsers, queues),
and delivers them to an *action* (e.g., write to file, forward, or database).

.. mermaid::

   flowchart LR
     A[Input] --> B[Ruleset]
     B --> C[Action]

This pipeline is the foundation of every tutorial in this series.

Some tutorials will also include:

- **Optional Docker shortcuts** – useful if Docker is already on your system and
  you want a quick sandbox environment.

- **Companion videos** – short screen-capture demos for steps that are easier to
  show than to describe (installation, first config, troubleshooting). Upcoming,
  not yet available.

.. note::

   The *video tips* included in tutorials exist because we plan to provide these
   short videos over time. We also highly appreciate community contributions —
   if you would like to record a companion video, please contact
   **rgerhards@adiscon.com** or post in the `GitHub discussions
   <https://github.com/rsyslog/rsyslog/discussions>`_.

----

.. toctree::
   :maxdepth: 1
   :numbered:

   01-installation
   02-first-config
   03-default-config
   04-message-pipeline
   05-order-matters
   06-remote-server

