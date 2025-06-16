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

   Parameter names are case-insensitive.


Action Parameters
-----------------

azurehost
^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "none", "yes", "none"

Specifies the fully qualified domain name (FQDN) of the Event Hubs instance that
the rsyslog output plugin should connect to. The format of the hostname should
be **<namespace>.servicebus.windows.net**, where **<namespace>** is the name
of the Event Hubs namespace that was created in Microsoft Azure.


azureport
^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "5671", "no", "none"

Specifies the TCP port number used by the Event Hubs instance for incoming connections.
The default port number for Event Hubs is 5671 for connections over the
AMQP Secure Sockets Layer (SSL) protocol. This property is usually optional in the configuration
file of the rsyslog output plugin, as the default value of 5671 is typically used.


azure_key_name
^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive", "Available since"
   :widths: auto
   :class: parameter-table

   "word", "none", "yes", "none"

The configuration property for the Azure key name used to connect to Microsoft Azure Event Hubs is
typically referred to as the "Event Hubs shared access key name". It specifies the name of
the shared access key that is used to authenticate and authorize connections to the Event Hubs instance.
The shared access key is a secret string that is used to securely sign and validate requests
to the Event Hubs instance.


azure_key
^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "none", "yes", "none"

The configuration property for the Azure key used to connect to Microsoft Azure Event Hubs is
typically referred to as the "Event Hubs shared access key". It specifies the value of the
shared access key that is used to authenticate and authorize connections to the Event Hubs instance.
The shared access key is a secret string that is used to securely sign and validate requests
to the Event Hubs instance.


container
^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "none", "yes", "none"

The configuration property for the Azure container used to connect to Microsoft Azure Event Hubs is
typically referred to as the "Event Hubs Instance". It specifies the name of the Event Hubs Instance,
to which log data should be sent.


template
^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "RSYSLOG_FileFormat", "no", "none"

Specifies the template used to format and structure the log messages that will be sent from rsyslog to
Microsoft Azure Event Hubs.

The message template can include rsyslog variables, such as the timestamp, hostname, or process name,
and it can use rsyslog macros, such as $rawmsg or $json, to control the formatting of log data. 

For a message template sample with valid JSON output see the sample below:

.. code-block:: none

	template(name="generic" type="list" option.jsonf="on") {
		property(outname="timestamp" name="timereported" dateFormat="rfc3339" format="jsonf")
		constant(value="\"source\": \"EventHubMessage\", ")
		property(outname="host" name="hostname" format="jsonf")
		property(outname="severity" name="syslogseverity" caseConversion="upper" format="jsonf" datatype="number")
		property(outname="facility" name="syslogfacility" format="jsonf" datatype="number")
		property(outname="appname" name="syslogtag" format="jsonf")
		property(outname="message" name="msg" format="jsonf" )
		property(outname="etlsource" name="$myhostname" format="jsonf")
	}


amqp_address
^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "none", "no", "none"

The configuration property for the AMQP address used to connect to Microsoft Azure Event Hubs is
typically referred to as the "Event Hubs connection string". It specifies the URL that is used to connect
to the target Event Hubs instance in Microsoft Azure. If the amqp_address is configured, the configuration 
parameters for **azurehost**, **azureport**, **azure_key_name** and **azure_key** will be ignored.

A sample Event Hubs connection string URL is:

.. code-block:: none

	amqps://[Shared access key name]:[Shared access key]@[Event Hubs namespace].servicebus.windows.net/[Event Hubs Instance]


eventproperties
^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "array", "none", "no", "none"

The **eventproperties** configuration property is an array property used to add key-value pairs as additional properties to the
encoded AMQP message object, providing additional information about the log event.
These properties can be used for filtering, routing, and grouping log events in Azure Event Hubs.

The event properties property is specified as a list of key-value pairs separated by comma,
with the key and value separated by an equal sign.

For example, the following configuration setting adds two event properties:

.. code-block:: none

	eventproperties=[	"Table=TestTable",
				"Format=JSON"]

In this example, the Table and Format keys are added to the message object as event properties,
with the corresponding values of TestTable and JSON, respectively.


closeTimeout
^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "2000", "no", "none"

The close timeout configuration property is used in the rsyslog output module
to specify the amount of time the output module should wait for a response
from Microsoft Azure Event Hubs before timing out and closing the connection.

This property is used to control the amount of time the output module will wait
for a response from the target Event Hubs instance before giving up and
assuming that the connection has failed. The close timeout property is specified in milliseconds.


statsname
^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "omazureeventhubs", "no", "none"

The name assigned to statistics specific to this action instance. The supported set of
statistics tracked for this action instance are **submitted**, **accepted**, **failures** and **failures_other**.
See the :ref:`statistics-counter_omazureeventhubs_label` section for more details.


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

