.. _param-omazureeventhubs-azure_key_name:
.. _omazureeventhubs.parameter.input.azure_key_name:

azure_key_name
==============

.. index::
   single: omazureeventhubs; azure_key_name
   single: azure_key_name

.. summary-start

Specifies the shared access key name used to authenticate with Event Hubs.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omazureeventhubs`.

:Name: azure_key_name
:Scope: input
:Type: word
:Default: input=none
:Required?: yes
:Introduced: v8.2304

Description
-----------
The configuration property for the Azure key name used to connect to Microsoft
Azure Event Hubs is typically referred to as the "Event Hubs shared access key
name". It specifies the name of the shared access key that is used to
authenticate and authorize connections to the Event Hubs instance.

Input usage
-----------
.. _omazureeventhubs.parameter.input.azure_key_name-usage:

.. code-block:: rsyslog

   action(type="omazureeventhubs" azureKeyName="MyPolicy" ...)

See also
--------
See also :doc:`../../configuration/modules/omazureeventhubs`.
