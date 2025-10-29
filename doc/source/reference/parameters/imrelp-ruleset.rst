.. _param-imrelp-ruleset:
.. _imrelp.parameter.module.ruleset:

Ruleset
=======

.. index::
   single: imrelp; Ruleset
   single: Ruleset

.. summary-start

Assigns a ruleset to all RELP listeners created by the module instance.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imrelp`.

:Name: Ruleset
:Scope: module
:Type: word
:Default: module=none
:Required?: no
:Introduced: 7.5.0

Description
-----------
Binds the specified ruleset to **all** RELP listeners. This can be overridden at
the instance level.

Module usage
------------
.. _param-imrelp-module-ruleset:
.. _imrelp.parameter.module.ruleset-usage:

.. code-block:: rsyslog

   module(load="imrelp" ruleset="relpRuleset")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.

.. _imrelp.parameter.legacy.inputrelpserverbindruleset:

- $InputRELPServerBindRuleset — maps to Ruleset (status: legacy)

.. index::
   single: imrelp; $InputRELPServerBindRuleset
   single: $InputRELPServerBindRuleset

See also
--------
See also :doc:`../../configuration/modules/imrelp`.
