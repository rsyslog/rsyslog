.. _param-imudp-ruleset:
.. _imudp.parameter.module.ruleset:

Ruleset
=======

.. index::
   single: imudp; Ruleset
   single: Ruleset

.. summary-start

Associates this listener with a specific :doc:`ruleset <../../concepts/multi_ruleset>`.

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
Binds received messages to the named ruleset, allowing per-listener processing pipelines.

Input usage
-----------
.. _param-imudp-input-ruleset:
.. _imudp.parameter.input.ruleset:
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
