.. _ref-mmjsonrewrite:

JSON Dotted Key Rewriter (mmjsonrewrite)
========================================

===========================  ==========================================================================
**Module Name:**             **mmjsonrewrite**
**Author:**                  `rsyslog project <https://github.com/rsyslog/rsyslog>`_
**Available since:**         8.2410.0
===========================  ==========================================================================

.. warning::

   ``mmjsonrewrite`` is **experimental**. Parameter names and behavior may
   change without notice while the interface stabilizes. Expect breaking
   changes in future releases.

Purpose
=======

``mmjsonrewrite`` scans a JSON object for property names that contain dot
characters and rewrites those entries into nested objects. This mirrors the
hierarchy that :doc:`list templates <../../configuration/templates>` create when
``option.jsonfTree`` is enabled, making it easier to normalize dotted payloads
before forwarding them to downstream processors.

Failure conditions
==================

``mmjsonrewrite`` aborts the action in the following situations:

* The input property does not resolve to a JSON object.
* The destination property already exists on the message.
* Rewriting a dotted path would overwrite an existing incompatible value.

Configuration Parameters
========================

.. note::

   Parameter names are case-insensitive. CamelCase is recommended for
   readability.

Action Parameters
-----------------

.. list-table::
   :widths: 30 70
   :header-rows: 1

   * - Parameter
     - Summary
   * - :ref:`param-mmjsonrewrite-input`
     - .. include:: ../../reference/parameters/mmjsonrewrite-input.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-mmjsonrewrite-output`
     - .. include:: ../../reference/parameters/mmjsonrewrite-output.rst
        :start-after: .. summary-start
        :end-before: .. summary-end

Conflict handling
=================

When dotted paths collide with an existing scalar or incompatible container,
``mmjsonrewrite`` stops processing, leaves the output unset, and logs an error
that includes the offending path. Review the reported path to adjust the
upstream payload or choose a different destination.

Examples
========

Normalize dotted JSON properties
--------------------------------

The following snippet converts dotted keys inside ``$!raw`` into nested
objects stored under ``$!structured``::

   module(load="mmjsonrewrite")
   module(load="imtcp")

   input(type="imtcp" port="10514" ruleset="rewrite-json")

   ruleset(name="rewrite-json") {
       action(type="mmjsonrewrite" input="$!raw" output="$!structured")
       action(type="omfile" file="/tmp/out.json"
              template="structured-json")
   }

   template(name="structured-json" type="string"
            string="%$!structured%\n")

Troubleshooting
===============

Conflicts usually stem from payloads that mix dotted keys with non-object values
at the same hierarchy. Inspect the logged path, rename the conflicting key, or
preprocess the message before running ``mmjsonrewrite``.

.. toctree::
   :hidden:

   ../../reference/parameters/mmjsonrewrite-input
   ../../reference/parameters/mmjsonrewrite-output
