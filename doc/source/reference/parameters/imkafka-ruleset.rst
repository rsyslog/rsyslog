.. _param-imkafka-ruleset:
.. _imkafka.parameter.input.ruleset:

Ruleset
=======

.. index::
   single: imkafka; Ruleset
   single: Ruleset

.. summary-start

Assigns the rsyslog ruleset that processes messages received via imkafka.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imkafka`.

:Name: Ruleset
:Scope: input
:Type: string
:Default: input=``none``
:Required?: no
:Introduced: 8.27.0

Description
-----------
Specifies the ruleset to be used.

Input usage
-----------
.. _imkafka.parameter.input.ruleset-usage:

.. code-block:: rsyslog

   module(load="imkafka")
   input(type="imkafka" topic="static" ruleset="kafkaRules")

See also
--------
See also :doc:`../../configuration/modules/imkafka`.
