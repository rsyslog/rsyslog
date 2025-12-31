.. _param-omruleset-ruleset:
.. _omruleset.parameter.input.ruleset:

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
:Scope: input
:Type: string
:Default: No default
:Required?: yes
:Introduced: 5.3.4

Description
-----------
This parameter specifies the name of the ruleset that the message provided
to omruleset should be submitted to. The target ruleset must already be
defined. This parameter is required for each ``omruleset`` action, which
helps prevent accidental misconfiguration and potential processing loops.

Input usage
-----------
.. _param-omruleset-ruleset-usage:
.. _omruleset.parameter.input.ruleset-usage:

.. code-block:: rsyslog

   module(load="omruleset")
   action(type="omruleset" ruleset="CommonAction")

Legacy names (for reference)
----------------------------
Historic names/directives for compatibility. Do not use in new configs.

.. _omruleset.parameter.legacy.actionomrulesetrulesetname:
- $ActionOmrulesetRulesetName — maps to Ruleset (status: legacy)

.. index::
   single: omruleset; $ActionOmrulesetRulesetName
   single: $ActionOmrulesetRulesetName

See also
--------
See also :doc:`../../configuration/modules/omruleset`.
