.. _param-omazureeventhubs-amqp_address:
.. _omazureeventhubs.parameter.input.amqp_address:

amqp_address
============

.. index::
   single: omazureeventhubs; amqp_address
   single: amqp_address

.. summary-start

Provides a full AMQPS connection string that overrides individual Azure settings.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omazureeventhubs`.

:Name: amqp_address
:Scope: input
:Type: word
:Default: input=none
:Required?: no
:Introduced: v8.2304

Description
-----------
Specifies the "Event Hubs connection string", a full AMQPS URL used to connect
to the target Event Hubs instance. When this parameter is set, the host, key
name, and key are parsed from the URL, overriding ``azureHost``,
``azureKeyName``, and ``azureKey``. The port is not parsed from the URL; it
always defaults to ``5671`` and any configured ``azurePort`` value is ignored.

A sample Event Hubs connection string URL is:

.. code-block:: none

   amqps://<SAS_KEY_NAME>:<SAS_KEY>@<NAMESPACE>.servicebus.windows.net/<EVENT_HUB_INSTANCE>

Input usage
-----------
.. _omazureeventhubs.parameter.input.amqp_address-usage:

.. code-block:: rsyslog

   action(type="omazureeventhubs" amqpAddress="amqps://user:key@namespace.servicebus.windows.net/hub" ...)

See also
--------
See also :doc:`../../configuration/modules/omazureeventhubs`.
