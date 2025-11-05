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
:Required?: no
:Introduced: v8.2304

Description
-----------
Specifies the name of the "Event Hubs Instance"—the specific event hub within
the namespace—that should receive the forwarded log data. Define it unless
``amqpAddress`` already points at the desired event hub.

Input usage
-----------
.. _omazureeventhubs.parameter.input.container-usage:

.. code-block:: rsyslog

   action(type="omazureeventhubs" container="MyEventHub" ...)

See also
--------
See also :doc:`../../configuration/modules/omazureeventhubs`.
