.. _param-imjournal-main:
.. _imjournal.parameter.input.main:

.. meta::
   :tag: module:imjournal
   :tag: parameter:Main

Main
====

.. index::
   single: imjournal; Main
   single: Main

.. summary-start

Runs the input's ruleset on the main thread and stops reading if outputs block.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imjournal`.

:Name: Main
:Scope: input
:Type: boolean
:Default: input=off
:Required?: no
:Introduced: 8.2312.0

Description
-----------
When enabled, the input module executes its bound ruleset in the main thread and
pauses ingestion if the output side cannot accept data. Only the first input with
``Main="on"`` is treated as such; others run in background threads.

Input usage
-----------
.. _param-imjournal-input-main:
.. _imjournal.parameter.input.main-usage:
.. code-block:: rsyslog

   input(type="imjournal" Main="...")

Notes
-----
- Earlier documentation misclassified this option; it is boolean.

See also
--------
See also :doc:`../../configuration/modules/imjournal`.
