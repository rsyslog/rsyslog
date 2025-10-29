.. _param-imrelp-ruleset-input:
.. _imrelp.parameter.input.ruleset:

ruleset
=======

.. index::
   single: imrelp; ruleset (input)
   single: ruleset (imrelp input)

.. summary-start

Overrides the module-wide ruleset binding for this specific RELP listener.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imrelp`.

:Name: ruleset
:Scope: input
:Type: string
:Default: input=none
:Required?: no
:Introduced: Not documented

Description
-----------
Binds the specified ruleset to this listener. This overrides the module-level
ruleset parameter.

Input usage
-----------
.. _param-imrelp-input-ruleset:
.. _imrelp.parameter.input.ruleset-usage:

.. code-block:: rsyslog

   input(type="imrelp" port="2514" ruleset="relpListenerRules")

See also
--------
See also :doc:`../../configuration/modules/imrelp`.
