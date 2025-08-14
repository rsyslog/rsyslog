.. _param-imfile-ruleset:
.. _imfile.parameter.module.ruleset:

Ruleset
=======

.. index::
   single: imfile; Ruleset
   single: Ruleset

.. summary-start

Binds the listener to a specific :doc:`ruleset <../../concepts/multi_ruleset>`.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imfile`.

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
.. _param-imfile-input-ruleset:
.. _imfile.parameter.input.ruleset:
.. code-block:: rsyslog

   input(type="imfile" Ruleset="...")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.

.. _imfile.parameter.legacy.inputfilebindruleset:
- $InputFileBindRuleset — maps to Ruleset (status: legacy)

.. index::
   single: imfile; $InputFileBindRuleset
   single: $InputFileBindRuleset

See also
--------
See also :doc:`../../configuration/modules/imfile`.
