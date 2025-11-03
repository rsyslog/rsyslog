.. _param-omazureeventhubs-azureport:
.. _omazureeventhubs.parameter.input.azureport:

azureport
=========

.. index::
   single: omazureeventhubs; azureport
   single: azureport

.. summary-start

Defines the TCP port used when connecting to the Event Hubs namespace.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omazureeventhubs`.

:Name: azureport
:Scope: input
:Type: word
:Default: input=5671
:Required?: no
:Introduced: v8.2304

Description
-----------
Specifies the TCP port number used by the Event Hubs instance for incoming
connections. The default port number for Event Hubs is 5671 for connections over
the AMQP Secure Sockets Layer (SSL) protocol. This property is usually optional
in the configuration file of the rsyslog output plugin, as the default value of
5671 is typically used.

Input usage
-----------
.. _omazureeventhubs.parameter.input.azureport-usage:

.. code-block:: rsyslog

   action(type="omazureeventhubs" azureHost="example.servicebus.windows.net" azurePort="5671" ...)

See also
--------
See also :doc:`../../configuration/modules/omazureeventhubs`.
