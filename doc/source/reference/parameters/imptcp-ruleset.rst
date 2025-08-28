.. _param-imptcp-ruleset:
.. _imptcp.parameter.input.ruleset:

Ruleset
=======

.. index::
   single: imptcp; Ruleset
   single: Ruleset

.. summary-start

Binds the specified ruleset to this input.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imptcp`.

:Name: Ruleset
:Scope: input
:Type: string
:Default: input=none
:Required?: no
:Introduced: at least 5.x, possibly earlier

Description
-----------
Binds specified ruleset to this input. If not set, the default
ruleset is bound.

Input usage
-----------
.. _param-imptcp-input-ruleset:
.. _imptcp.parameter.input.ruleset-usage:

.. code-block:: rsyslog

   input(type="imptcp" ruleset="...")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.

.. _imptcp.parameter.legacy.inputptcpserverbindruleset:

- $InputPTCPServerBindRuleset — maps to Ruleset (status: legacy)

.. index::
   single: imptcp; $InputPTCPServerBindRuleset
   single: $InputPTCPServerBindRuleset

See also
--------
See also :doc:`../../configuration/modules/imptcp`.
