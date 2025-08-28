.. _param-imtcp-ruleset:
.. _imtcp.parameter.input.ruleset:

Ruleset
=======

.. index::
   single: imtcp; Ruleset
   single: Ruleset

.. summary-start

Binds the listener to a specific ruleset.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imtcp`.

:Name: Ruleset
:Scope: input
:Type: string (see :doc:`../../rainerscript/constant_strings`)
:Default: input=none
:Required?: no
:Introduced: at least 5.x, possibly earlier

Description
-----------
Binds the listener to a specific :doc:`ruleset <../../concepts/multi_ruleset>`.

Input usage
-----------
.. _param-imtcp-input-ruleset:
.. _imtcp.parameter.input.ruleset-usage:

.. code-block:: rsyslog

   input(type="imtcp" port="514" ruleset="remote")

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
