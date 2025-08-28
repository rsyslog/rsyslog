.. _param-imuxsock-ruleset:
.. _imuxsock.parameter.input.ruleset:

Ruleset
=======

.. index::
   single: imuxsock; Ruleset
   single: Ruleset

.. summary-start

Binds a specified ruleset to the input instead of the default ruleset.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imuxsock`.

:Name: Ruleset
:Scope: input
:Type: string
:Default: input=default ruleset
:Required?: no
:Introduced: 8.17.0

Description
-----------
Binds specified ruleset to this input. If not set, the default
ruleset is bound.

Input usage
-----------
.. _param-imuxsock-input-ruleset:
.. _imuxsock.parameter.input.ruleset-usage:

.. code-block:: rsyslog

   input(type="imuxsock" ruleset="myRuleset")

See also
--------
See also :doc:`../../configuration/modules/imuxsock`.
