.. _param-mmnormalize-rule:
.. _mmnormalize.parameter.action.rule:

rule
====

.. index::
   single: mmnormalize; rule
   single: rule

.. summary-start

Builds the rulebase from an array of strings.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/mmnormalize`.

:Name: rule
:Scope: action
:Type: array
:Default: none
:Required?: yes
:Introduced: 8.26.0

Description
-----------
.. versionadded:: 8.26.0

Contains an array of strings which will be put together as the rulebase. This parameter or **ruleBase** MUST be given, because normalization can only happen based on a rulebase.

Action usage
-------------
.. _param-mmnormalize-action-rule:
.. _mmnormalize.parameter.action.rule-usage:

.. code-block:: rsyslog

   action(type="mmnormalize" rule=["rule=:%host:word% %ip:ipv4% user was logged out"])

See also
--------
See also :doc:`../../configuration/modules/mmnormalize`.
