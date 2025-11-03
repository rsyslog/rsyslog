.. _param-omazureeventhubs-azurehost:
.. _omazureeventhubs.parameter.module.azurehost:

azurehost
=========

.. index::
   single: omazureeventhubs; azurehost
   single: azurehost

.. summary-start

Sets the fully qualified Event Hubs namespace host the action connects to.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omazureeventhubs`.

:Name: azurehost
:Scope: module
:Type: word
:Default: module=none
:Required?: yes
:Introduced: v8.2304

Description
-----------
Specifies the fully qualified domain name (FQDN) of the Event Hubs instance that the
rsyslog output plugin should connect to. The format of the hostname should be
``<namespace>.servicebus.windows.net``, where ``<namespace>`` is the name of the
Event Hubs namespace that was created in Microsoft Azure.

Module usage
------------
.. _param-omazureeventhubs-module-azurehost:
.. _omazureeventhubs.parameter.module.azurehost-usage:

.. code-block:: rsyslog

   action(type="omazureeventhubs" azureHost="example.servicebus.windows.net" ...)

See also
--------
See also :doc:`../../configuration/modules/omazureeventhubs`.
