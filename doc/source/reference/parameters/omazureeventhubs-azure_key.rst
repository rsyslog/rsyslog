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
:Required?: yes
:Introduced: v8.2304

Description
-----------
The configuration property for the Azure key used to connect to Microsoft Azure
Event Hubs is typically referred to as the "Event Hubs shared access key". It
specifies the value of the shared access key that is used to authenticate and
authorize connections to the Event Hubs instance. The shared access key is a
secret string that is used to securely sign and validate requests to the Event
Hubs instance.

Input usage
-----------
.. _omazureeventhubs.parameter.input.azure_key-usage:

.. code-block:: rsyslog

   action(type="omazureeventhubs" azureKeyName="MyPolicy" azureKey="MySecretKey" ...)

See also
--------
See also :doc:`../../configuration/modules/omazureeventhubs`.
