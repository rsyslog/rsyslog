.. _param-imkubernetes-ruleset:
.. _imkubernetes.parameter.input.ruleset:

Ruleset
=======

.. index::
   single: imkubernetes; Ruleset
   single: Ruleset

.. summary-start

Ruleset that receives records submitted by ``imkubernetes``; default is the
default ruleset.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imkubernetes`.

:Name: Ruleset
:Scope: module, input
:Type: string
:Default: default ruleset
:Required?: no
:Introduced: 8.2606.0

Description
-----------
Bind ``imkubernetes`` records to the named ruleset. This is especially useful
with YAML configuration, where the input object can name the ruleset that owns
the output actions.

Input usage
-----------

.. code-block:: rsyslog

   module(load="imkubernetes" Ruleset="kubernetes")

.. code-block:: rsyslog

   input(type="imkubernetes" Ruleset="kubernetes")

See also
--------
See also :doc:`../../configuration/modules/imkubernetes`.
