.. _param-imudp-ruleset:
.. _imudp.parameter.input.ruleset:

Ruleset
=======

.. index::
   single: imudp; Ruleset
   single: Ruleset

.. summary-start

Assigns incoming messages to a specified ruleset.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imudp`.

:Name: Ruleset
:Scope: input
:Type: string (see :doc:`../../rainerscript/constant_strings`)
:Default: input=RSYSLOG_DefaultRuleset
:Required?: no
:Introduced: at least 5.x, possibly earlier

Description
-----------
Binds the listener to a specific :doc:`ruleset <../../concepts/multi_ruleset>`.

Input usage
-----------
.. _param-imudp-input-ruleset:
.. _imudp.parameter.input.ruleset-usage:

.. code-block:: rsyslog

   input(type="imudp" Ruleset="...")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.

.. _imudp.parameter.legacy.inputudpserverbindruleset:

- $InputUDPServerBindRuleset — maps to Ruleset (status: legacy)

.. index::
   single: imudp; $InputUDPServerBindRuleset
   single: $InputUDPServerBindRuleset

See also
--------
See also :doc:`../../configuration/modules/imudp`.
