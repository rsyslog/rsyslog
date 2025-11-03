.. _param-omazureeventhubs-eventproperties:
.. _omazureeventhubs.parameter.input.eventproperties:

eventproperties
===============

.. index::
   single: omazureeventhubs; eventproperties
   single: eventproperties

.. summary-start

Adds custom key-value properties to each AMQP message emitted by the action.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omazureeventhubs`.

:Name: eventproperties
:Scope: input
:Type: array
:Default: input=none
:Required?: no
:Introduced: v8.2304

Description
-----------
The ``eventproperties`` configuration property is an array property used to add
key-value pairs as additional properties to the encoded AMQP message object,
providing additional information about the log event. These properties can be
used for filtering, routing, and grouping log events in Azure Event Hubs.

The event properties property is specified as a list of key-value pairs
separated by comma, with the key and value separated by an equal sign.

For example, the following configuration adds two event properties inside an
action definition:

.. code-block:: rsyslog

   action(
       type="omazureeventhubs"
       # ... other parameters
       eventProperties=[ "Table=TestTable",
                         "Format=JSON" ]
   )

In this example, the Table and Format keys are added to the message object as
event properties, with the corresponding values of ``TestTable`` and ``JSON``,
respectively.

Input usage
-----------
.. _omazureeventhubs.parameter.input.eventproperties-usage:

.. code-block:: rsyslog

   action(type="omazureeventhubs" eventProperties=["Table=TestTable","Format=JSON"] ...)

See also
--------
See also :doc:`../../configuration/modules/omazureeventhubs`.
