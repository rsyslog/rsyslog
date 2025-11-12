.. _ref-mmjsontransform:

JSON Dotted Key Transformer (mmjsontransform)
=============================================

===========================  ==========================================================================
**Module Name:**             **mmjsontransform**
**Author:**                  `rsyslog project <https://github.com/rsyslog/rsyslog>`_
**Available since:**         8.2410.0
===========================  ==========================================================================

.. note::

   ``mmjsontransform`` is a **fresh** module. Parameter names and behavior may
   still change as real-world feedback arrives, so expect possible breaking
   adjustments in upcoming releases.

Purpose
=======

``mmjsontransform`` restructures JSON properties whose names contain dotted
segments. The action reads a JSON object from the configured input property and
stores the transformed tree under a dedicated output property. By default the
module expands dotted keys into nested containers (``unflatten`` mode) so
pipelines that consume ``option.jsonfTree`` data can normalize payloads inline.
When ``mode="flatten"`` is selected, the action collapses nested objects back
into dotted keys.

Failure conditions
==================

``mmjsontransform`` aborts the action when any of the following occur:

* The input property does not resolve to a JSON object.
* The output property already exists on the message.
* Rewriting a key would overwrite an existing incompatible value (for
  example, ``{"a": 1}`` combined with ``{"a.b": 2}``).

Notable Features
================

- :ref:`mmjsontransform-modes` — bidirectional conversion between dotted keys
  and nested containers.
- :ref:`mmjsontransform-conflict-handling` — detailed conflict reporting to
  locate incompatible payloads quickly.

Configuration Parameters
========================

.. note::

   Parameter names are case-insensitive. For readability, camelCase is
   recommended.

Action Parameters
-----------------

.. list-table::
   :widths: 30 70
   :header-rows: 1

   * - Parameter
     - Summary
   * - :ref:`param-mmjsontransform-input`
     - .. include:: ../../reference/parameters/mmjsontransform-input.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-mmjsontransform-output`
     - .. include:: ../../reference/parameters/mmjsontransform-output.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-mmjsontransform-mode`
     - .. include:: ../../reference/parameters/mmjsontransform-mode.rst
        :start-after: .. summary-start
        :end-before: .. summary-end

.. _mmjsontransform-modes:

Transformation modes
====================

``mmjsontransform`` supports two modes controlled by the :ref:`mode
<param-mmjsontransform-mode>` parameter. Both modes rewrite the entire input
object before assigning it to the configured output property.

.. _mmjsontransform-mode-unflatten:

Unflatten dotted keys
---------------------

``mode="unflatten"`` (the default) expands dotted keys into nested containers.
For example ``{"a.b": 1, "a.c": 2}`` becomes ``{"a": {"b": 1, "c": 2}}``.
When the module encounters a dotted path that points to an existing
non-object value, it stops processing, leaves the output unset, and reports the
conflicting path.

.. _mmjsontransform-mode-flatten:

Flatten nested containers
-------------------------

``mode="flatten"`` performs the reverse operation. Nested objects become dotted
key paths (``{"nested": {"value": 1}}`` is rewritten to
``{"nested.value": 1}``), while arrays are preserved with their elements
recursively flattened. If flattening would overwrite an existing scalar with a
different value, the action fails and reports the mismatch.

.. _mmjsontransform-conflict-handling:

Conflict handling
=================

The module tracks the full dotted path responsible for a hierarchy conflict and
logs a user-facing error when a mismatch occurs. Review the reported path to
identify which input property must be renamed or preprocessed before retrying
the transformation.

Examples
========

Normalize and persist dotted JSON
---------------------------------

The following snippet receives JSON payloads, normalizes dotted keys into
nested objects, and writes both the structured tree and a flattened
representation to ``/tmp/out.json``::

   module(load="mmjsontransform")
   module(load="imtcp")

   input(type="imtcp" port="10514" ruleset="process-json")

   ruleset(name="process-json") {
       action(type="mmjsontransform" input="$!raw" output="$!normalized")
       action(type="mmjsontransform" mode="flatten"
              input="$!normalized" output="$!normalized_flat")
       action(type="omfile" file="/tmp/out.json"
              template="json-template")
   }

   template(name="json-template" type="string"
            string="%$!normalized%\n%$!normalized_flat%\n")

Troubleshooting
===============

When a hierarchy conflict occurs (for example, ``mode="unflatten"`` sees
``"a": "value"`` and ``"a.b": 1``), ``mmjsontransform`` logs an error that
includes the conflicting path. Inspect the reported path to determine which
input property needs to be renamed or moved before retrying the transformation.

.. toctree::
   :hidden:

   ../../reference/parameters/mmjsontransform-input
   ../../reference/parameters/mmjsontransform-output
   ../../reference/parameters/mmjsontransform-mode
