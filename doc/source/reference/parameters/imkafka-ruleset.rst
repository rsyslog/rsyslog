.. _param-imkafka-ruleset:
.. _imkafka.parameter.module.ruleset:
.. _imkafka.parameter.input.ruleset:

ruleset
=======

.. index::
   single: imkafka; ruleset
   single: ruleset

.. summary-start

Assigns the rsyslog ruleset that processes messages received via imkafka.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imkafka`.

:Name: ruleset
:Scope: module, input
:Type: string
:Default: module=``none``; input=``none``
:Required?: no
:Introduced: 8.27.0

Description
-----------
Binds the input to a specific ruleset for processing. If not specified,
messages are passed to the default ruleset. When set at the module level,
the value becomes the default ruleset for inputs that do not override it.

Module usage
------------
.. _imkafka.parameter.module.ruleset-usage:

.. code-block:: rsyslog

   module(load="imkafka" ruleset="kafkaRules")

   # This input will use 'kafkaRules' ruleset
   input(type="imkafka" topic="my-topic")

Input usage
-----------
.. _imkafka.parameter.input.ruleset-usage:

.. code-block:: rsyslog

   module(load="imkafka")
   input(type="imkafka" topic="static" ruleset="kafkaRules")

See also
--------
See also :doc:`../../configuration/modules/imkafka`.
