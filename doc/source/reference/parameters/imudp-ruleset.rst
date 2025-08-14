.. _param-imudp-ruleset:
.. _imudp.parameter.module.ruleset:

Ruleset
=======

.. index::
   single: imudp; Ruleset
   single: Ruleset

.. summary-start

Binds the listener to a specific rsyslog ruleset.

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
Associates the listener with the given :doc:`ruleset <../../concepts/multi_ruleset>`.
Messages received on this input are directed to that ruleset for processing.

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

