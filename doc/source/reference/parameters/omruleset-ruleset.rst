.. _param-omruleset-ruleset:
.. _omruleset.parameter.module.ruleset:

Ruleset
=======

.. index::
   single: omruleset; Ruleset
   single: Ruleset

.. summary-start

Specifies the target ruleset that omruleset should submit messages to.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omruleset`.

:Name: Ruleset
:Scope: module
:Type: string
:Default: module=no default
:Required?: yes
:Introduced: 5.3.4

Description
-----------
This parameter specifies the name of the ruleset that the message provided to omruleset should be submitted to. The ruleset must already exist. The directive is automatically reset after each :omruleset: action and there is no default. This behavior helps prevent accidental loops in ruleset definition. The :omruleset: action is ignored if no ruleset name has been defined. As usual, the ruleset name must be specified in front of the action that it modifies.

Module usage
------------
.. _omruleset.parameter.module.ruleset-usage:

.. code-block:: rsyslog

   module(load="omruleset")
   action(type="omruleset" ruleset="CommonAction")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.

.. _omruleset.parameter.legacy.actionomrulesetrulesetname:
- $ActionOmrulesetRulesetName — maps to Ruleset (status: legacy)

.. index::
   single: omruleset; $ActionOmrulesetRulesetName
   single: $ActionOmrulesetRulesetName

See also
--------
See also :doc:`../../configuration/modules/omruleset`.
