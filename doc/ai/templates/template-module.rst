.. _module-template:

================================
Module Page Template (im/om/mm)
================================

.. index:: ! module-name

.. meta::
   :description: <≤160 chars>
   :keywords: rsyslog, module-name, input/output/processor, key-topic

.. summary-start

Short one-paragraph summary (≤200 chars) describing what the module does.

.. summary-end

===========================  ==========================================================
**Module Name:**             **module-name**
**Author/Maintainer:**       Name <email> (or project/organization)
**Introduced:**              Version or release note (if known)
===========================  ==========================================================

Purpose
=======

State the module's primary responsibility and where it fits in the pipeline.

Main Features
=============

- Feature one (short clause).
- Feature two.
- Compatibility or runtime constraints if critical.

Configuration Parameters
========================

Use the parameter includes from `doc/source/reference/parameters/` when
available.

Example
=======

.. code-block:: rsyslog

   module(load="module-name")
   # input/action example goes here

Notes
=====

Mention caveats, performance considerations, or dependencies.

See also
========

- :doc:`../configuration/index`
