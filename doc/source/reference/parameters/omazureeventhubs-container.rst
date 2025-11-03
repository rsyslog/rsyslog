.. _param-omazureeventhubs-container:
.. _omazureeventhubs.parameter.input.container:

container
=========

.. index::
   single: omazureeventhubs; container
   single: container

.. summary-start

Identifies the Event Hubs instance that receives the formatted log messages.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omazureeventhubs`.

:Name: container
:Scope: input
:Type: word
:Default: input=none
:Required?: yes
:Introduced: v8.2304

Description
-----------
The configuration property for the Azure container used to connect to Microsoft
Azure Event Hubs is typically referred to as the "Event Hubs Instance". It
specifies the name of the Event Hubs Instance to which log data should be sent.

Input usage
-----------
.. _omazureeventhubs.parameter.input.container-usage:

.. code-block:: rsyslog

   action(type="omazureeventhubs" container="MyEventHub" ...)

See also
--------
See also :doc:`../../configuration/modules/omazureeventhubs`.
