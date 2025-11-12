.. _param-omazureeventhubs-azure_key:
.. _omazureeventhubs.parameter.input.azure_key:

azure_key
=========

.. index::
   single: omazureeventhubs; azure_key
   single: azure_key

.. summary-start

Provides the shared access key value used to sign Event Hubs requests.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omazureeventhubs`.

:Name: azure_key
:Scope: input
:Type: word
:Default: input=none
:Required?: no
:Introduced: v8.2304

Description
-----------
Specifies the value of the "Event Hubs shared access key". This secret key
string authenticates the connection and signs requests to the Event Hubs
instance. Supply it unless ``amqpAddress`` already embeds the key.

Input usage
-----------
.. _omazureeventhubs.parameter.input.azure_key-usage:

.. code-block:: rsyslog

   action(type="omazureeventhubs" azureKeyName="MyPolicy" azureKey="MySecretKey" ...)

See also
--------
See also :doc:`../../configuration/modules/omazureeventhubs`.
