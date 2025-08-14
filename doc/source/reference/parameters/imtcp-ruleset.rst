.. _param-imtcp-ruleset:
.. _imtcp.parameter.input.ruleset:

Ruleset
=======

.. index::
   single: imtcp; Ruleset
   single: Ruleset

.. summary-start

Binds the input listener to a specific ruleset.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imtcp`.

:Name: Ruleset
:Scope: input
:Type: string
:Default: none
:Required?: no
:Introduced: at least 5.x, possibly earlier

Description
-----------
This parameter binds the listener to a specific :doc:`ruleset <../../concepts/multi_ruleset>`. All messages received through this listener will be processed by the specified ruleset.

Input usage
-----------
.. _imtcp.parameter.input.ruleset-usage:

.. code-block:: rsyslog

   input(type="imtcp" port="514" ruleset="my-ruleset")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.

.. _imtcp.parameter.legacy.inputtcpserverbindruleset:
- $InputTCPServerBindRuleset — maps to Ruleset (status: legacy)

.. index::
   single: imtcp; $InputTCPServerBindRuleset
   single: $InputTCPServerBindRuleset

See also
--------
See also :doc:`../../configuration/modules/imtcp`.
