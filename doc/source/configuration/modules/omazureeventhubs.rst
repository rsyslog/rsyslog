**********************************************************
omazureeventhubs: Microsoft Azure Event Hubs Output Module
**********************************************************

===========================  ===========================================================================
**Module Name:**             **omazureeventhubs**
**Author:**                  Andre Lorbach <alorbach@adiscon.com>
**Available since:**         v8.2304
===========================  ===========================================================================


Purpose
=======

The purpose of the rsyslog output plugin omazureeventhubs is to provide a
fast and reliable way to send log data from rsyslog to Microsoft Azure Event Hubs.
This plugin uses the Advanced Message Queuing Protocol (AMQP) to securely transmit
log data from rsyslog to Microsoft Azure, where it can be centralized, analyzed, and stored.
The plugin uses the "Qpid Proton C API" library to implement the AMQP protocol,
providing a flexible and efficient solution for sending log data to Microsoft Azure Event Hubs.

AMQP is a reliable and secure binary protocol for exchanging messages between applications,
and it is widely used in the cloud and enterprise messaging systems. The use of AMQP in the
omazureeventhubs plugin, in combination with the Qpid Proton C API library, ensures that
log data is transmitted in a robust and reliable manner, even in the presence of network
outages or other disruptions.

The omazureeventhubs plugin supports various configuration options, allowing organizations to
customize their log data pipeline to meet their specific requirements.
This includes options for specifying the Event Hubs endpoint, port, and authentication credentials.
With this plugin, organizations can easily integrate their rsyslog infrastructure with
Microsoft Azure Event Hubs, providing a scalable and secure solution for log management.
The plugin is designed to work with the latest versions of rsyslog and Microsoft Azure,
ensuring compatibility and reliability.


Requirements
============

To output logs from rsyslog to Microsoft Azure Event Hubs, you will need to fulfill the 
following requirements:

-  Qpid Proton C Library Version 0.13 or higher including Qpid Proton ProActor
-  The AMQP Protocol needs to have firewall Ports 5671 and 443 TCP to be open for outgoing connections.


Configuration Parameters
========================

.. note::

   Parameter names are case-insensitive; camelCase is recommended for readability.


Action Parameters
-----------------

.. list-table::
   :widths: 30 70
   :header-rows: 1

   * - Parameter
     - Summary
   * - :ref:`param-omazureeventhubs-azurehost`
     - .. include:: ../../reference/parameters/omazureeventhubs-azurehost.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omazureeventhubs-azureport`
     - .. include:: ../../reference/parameters/omazureeventhubs-azureport.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omazureeventhubs-azure_key_name`
     - .. include:: ../../reference/parameters/omazureeventhubs-azure_key_name.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omazureeventhubs-azure_key`
     - .. include:: ../../reference/parameters/omazureeventhubs-azure_key.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omazureeventhubs-container`
     - .. include:: ../../reference/parameters/omazureeventhubs-container.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omazureeventhubs-template`
     - .. include:: ../../reference/parameters/omazureeventhubs-template.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omazureeventhubs-amqp_address`
     - .. include:: ../../reference/parameters/omazureeventhubs-amqp_address.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omazureeventhubs-eventproperties`
     - .. include:: ../../reference/parameters/omazureeventhubs-eventproperties.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omazureeventhubs-closetimeout`
     - .. include:: ../../reference/parameters/omazureeventhubs-closetimeout.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omazureeventhubs-statsname`
     - .. include:: ../../reference/parameters/omazureeventhubs-statsname.rst
        :start-after: .. summary-start
        :end-before: .. summary-end

.. toctree::
   :hidden:

   ../../reference/parameters/omazureeventhubs-azurehost
   ../../reference/parameters/omazureeventhubs-azureport
   ../../reference/parameters/omazureeventhubs-azure_key_name
   ../../reference/parameters/omazureeventhubs-azure_key
   ../../reference/parameters/omazureeventhubs-container
   ../../reference/parameters/omazureeventhubs-template
   ../../reference/parameters/omazureeventhubs-amqp_address
   ../../reference/parameters/omazureeventhubs-eventproperties
   ../../reference/parameters/omazureeventhubs-closetimeout
   ../../reference/parameters/omazureeventhubs-statsname

.. _statistics-counter_omazureeventhubs_label:

Statistic Counter
=================

This plugin maintains global :doc:`statistics <../rsyslog_statistic_counter>` for omazureeventhubs that
accumulate all action instances. The statistic origin is named "omazureeventhubs" with following counters:


- **submitted** - This counter tracks the number of log messages that have been submitted by the rsyslog process
  to the output module for delivery to Microsoft Azure Event Hubs.

- **accepted** - This counter tracks the number of log messages that have been successfully delivered to
  Microsoft Azure Event Hubs by the output module.

- **failures** - This counter tracks the number of log messages that have failed to be delivered to 
  Microsoft Azure Event Hubs due to various error conditions, such as network connectivity issues, 
  incorrect configuration settings, or other technical problems. This counter provides important information about 
  any issues that may be affecting the delivery of log data to Microsoft Azure Event Hubs.

- **failures_other** - This counter tracks the number of log messages that have failed to be delivered due to 
  other error conditions, such as incorrect payload format or unexpected data.

These statistics counters are updated in real-time by the rsyslog output module as log data is processed,
and they provide valuable information about the performance and operation of the output module.

For multiple actions using statistics callback, there will be one record for each action.

.. _omazureeventhubs-examples-label:

Examples
========

Example 1: Use AMQP URL
-----------------------

The following sample does the following:

-  loads the omazureeventhubs module
-  outputs all logs to Microsoft Azure Event Hubs with standard template
-  Uses amqp_address parameter

.. code-block:: none

   module(load="omazureeventhubs")
   action(type="omazureeventhubs" amqp_address="amqps://<AccessKeyName>:<AccessKey>@<EventHubsNamespace>.servicebus.windows.net/<EventHubsInstance>")


Example 2: RAW Format
---------------------

The following sample does the following:

-  loads the omazureeventhubs module
-  outputs all logs to Microsoft Azure Event Hubs with simple custom template
-  Uses **azurehost**, **azureport**, **azure_key_name** and **azure_key** 
   parameters instead of **amqp_address** parameter

.. code-block:: none

   module(load="omazureeventhubs")
   template(name="outfmt" type="string" string="%msg%\n")

   action(type="omazureeventhubs" 
	azurehost="<EventHubsNamespace>.servicebus.windows.net"
	azureport="5671"
	azure_key_name="<AccessKeyName>"
	azure_key="<AccessKey>"
	container="<EventHubsInstance>"
	template="outfmt"
   )


Example 3: JSON Format
----------------------

The following sample does the following:

-  loads the omazureeventhubs module
-  outputs all logs to Microsoft Azure Event Hubs with JSON custom template
-  Uses **azurehost**, **azureport**, **azure_key_name** and **azure_key** 
   parameters instead of **amqp_address** parameter
-  Uses **eventproperties** array parameter to set additional message properties

.. code-block:: none

   module(load="omazureeventhubs")
   template(name="outfmtjson" type="list" option.jsonf="on") {
	property(outname="timestamp" name="timereported" dateFormat="rfc3339" format="jsonf")
	constant(value="\"source\": \"EventHubMessage\", ")
	property(outname="host" name="hostname" format="jsonf")
	property(outname="severity" name="syslogseverity" caseConversion="upper" format="jsonf" datatype="number")
	property(outname="facility" name="syslogfacility" format="jsonf" datatype="number")
	property(outname="appname" name="syslogtag" format="jsonf")
	property(outname="message" name="msg" format="jsonf" )
	property(outname="etlsource" name="$myhostname" format="jsonf")
   }

   action(type="omazureeventhubs" 
	azurehost="<EventHubsNamespace>.servicebus.windows.net"
	azureport="5671"
	azure_key_name="<AccessKeyName>"
	azure_key="<AccessKey>"
	container="<EventHubsInstance>"
	template="outfmtjson"
	eventproperties=[	"Table=CustomTable",
				"Format=JSON"]
   )

Example 4: High Performance
---------------------------

To achieve high performance when sending syslog data to Azure Event Hubs, you should consider configuring your output module to use multiple worker instances. This can be done by setting the "workerthreads" parameter in the configuration file.

The following example is for high performance (Azure Premium Tier) and does the following:

-  loads the omazureeventhubs module
-  outputs all logs to Microsoft Azure Event Hubs with JSON custom template
-  Uses **azurehost**, **azureport**, **azure_key_name** and **azure_key** 
   parameters instead of **amqp_address** parameter
-  Uses **eventproperties** array parameter to set additional message properties
-  Uses **Linkedlist** In-Memory Queue which enables multiple omazureeventhubs workers running at the same time. Using a dequeue size of 2000 and a dequeue timeout of 1000 has shown very good results in performance tests.
-  Uses 8 workerthreads in this example, which will be spawn automatically if more than 2000 messages are waiting in the Queue. To achieve more performance, the number can be incremented.

.. code-block:: none

   module(load="omazureeventhubs")
   template(name="outfmtjson" type="list" option.jsonf="on") {
	property(outname="timestamp" name="timereported" dateFormat="rfc3339" format="jsonf")
	constant(value="\"source\": \"EventHubMessage\", ")
	property(outname="host" name="hostname" format="jsonf")
	property(outname="severity" name="syslogseverity" caseConversion="upper" format="jsonf" datatype="number")
	property(outname="facility" name="syslogfacility" format="jsonf" datatype="number")
	property(outname="appname" name="syslogtag" format="jsonf")
	property(outname="message" name="msg" format="jsonf" )
	property(outname="etlsource" name="$myhostname" format="jsonf")
   }

   action(type="omazureeventhubs" 
	azurehost="<EventHubsNamespace>.servicebus.windows.net"
	azureport="5671"
	azure_key_name="<AccessKeyName>"
	azure_key="<AccessKey>"
	container="<EventHubsInstance>"
	template="outfmtjson"
	eventproperties=[	"Table=CustomTable",
				"Format=JSON"]
	queue.type="linkedList"
	queue.size="200000"
	queue.saveonshutdown="on"
	queue.dequeueBatchSize="2000"
	queue.minDequeueBatchSize.timeout="1000"
	queue.workerThreads="8"
	queue.workerThreadMinimumMessages="2000"
	queue.timeoutWorkerthreadShutdown="10000"
	queue.timeoutshutdown="1000"
   )

