.. _param-imbeats-ruleset:
.. _imbeats.parameter.input.ruleset:

ruleset
========

.. meta::
   :description: Ruleset binding for imbeats.
   :keywords: rsyslog, imbeats, ruleset

.. index::
   single: imbeats; ruleset
   single: ruleset

.. summary-start

Bind the imbeats input to a specific ruleset instead of the default ruleset.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imbeats`.

:Name: ruleset
:Scope: input
:Type: string
:Default: input=default ruleset
:Required?: no
:Introduced: 8.2604.0

Description
-----------
Bind the imbeats input to a specific ruleset instead of the default ruleset.

Input usage
-----------
.. _param-imbeats-input-ruleset:
.. _imbeats.parameter.input.ruleset-usage:

.. code-block:: rsyslog

   input(type="imbeats" port="5044" ruleset="beats")

See also
--------
See also :doc:`../../configuration/modules/imbeats`.
