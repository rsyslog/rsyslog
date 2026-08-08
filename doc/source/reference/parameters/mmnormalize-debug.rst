.. _param-mmnormalize-debug:
.. _mmnormalize.parameter.action.debug:

.. meta::
   :description: Enable per-action liblognorm debug tracing for mmnormalize.
   :keywords: rsyslog, mmnormalize, liblognorm, debug, tracing

debug
=====

.. index::
   single: mmnormalize; debug
   single: debug

.. summary-start

Enables per-action liblognorm debug tracing to rsyslog diagnostics or a
configured debug file.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/mmnormalize`.

:Name: debug
:Scope: action
:Type: boolean
:Default: off
:Required?: no
:Introduced: 8.2608.0

Description
-----------
Enables verbose liblognorm debug records for this action. Without
:ref:`param-mmnormalize-debugfile`, rsyslog emits the records through its
internal diagnostic stream. Use ``debugFile`` when trace records should be
kept separately from rsyslog diagnostics.

Action usage
------------
.. _param-mmnormalize-action-debug:
.. _mmnormalize.parameter.action.debug-usage:

.. code-block:: rsyslog

   action(type="mmnormalize"
          rulebase="/path/to/rules.rb"
          debug="on")

YAML usage
----------

.. code-block:: yaml

   actions:
     - type: mmnormalize
       rulebase: /path/to/rules.rb
       debug: on

See also
--------
See also :doc:`../../configuration/modules/mmnormalize` and
:ref:`param-mmnormalize-debugfile`.
