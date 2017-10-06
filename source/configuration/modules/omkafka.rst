omkafka: write to Apache Kafka
==============================

===========================  ===========================================================================
**Module Name:**             **omkafka**
**Author:**                  `Rainer Gerhards <http://www.gerhards.net/rainer>`_ <rgerhards@adiscon.com>
**Available since:**         v8.7.0
===========================  ===========================================================================

The omkafka plug-in implements an Apache Kafka producer, permitting
rsyslog to write data to Kafka.

Configuration Parameters
------------------------

Module Parameters
^^^^^^^^^^^^^^^^^
Currently none.


Action Parameters
^^^^^^^^^^^^^^^^^
Note that omkafka supports some *Array*-type parameters. While the parameter
name can only be set once, it is possible to set multiple values with that
single parameter.

For example, to select "snappy" compression, you can use

::

   action(type="omkafka" topic="mytopic" confParam="compression.codec=snappy")

which is equivalent to

::

   action(type="omkafka" topic="mytopic" confParam=["compression.codec=snappy"])

To specify multiple values, just use the bracket notation and create a
comma-delimited list of values as shown here:

::

   action(type="omkafka" topic="mytopic"
          confParam=["compression.codec=snappy",
	             "socket.timeout.ms=5",
		     "socket.keepalive.enable=true"]
         )

.. function::  broker [brokers]

   *Type:* Array

   *Default: "localhost:9092"*

   Specifies the broker(s) to use.

.. function::  topic [topicName]

   *Mandatory*

   Specifies the topic to produce to.

.. function::  key [key]

   *Default: none*

   Kafka key to be used for all messages.

.. function::  dynatopic [boolean]

   *Default: off*

   If set, the topic parameter becomes a template for which topic to
   produce messages to. The cache is cleared on HUP.

.. function::  dynatopic.cachesize [positiveInteger]

   *Default: 50*

   If set, defines the number of topics that will be kept in the dynatopic
   cache.

.. function::  partitions.auto [boolean]

   *Default: off*

   librdkafka provides an automatic partitioning function that will
   evenly split the produced messages into all partitions configured
   for that topic.

   To use, set partitions.auto="on". This is instead of specifying the
   number of partitions on the producer side, where it would be easier
   to change the kafka configuration on the cluster for number of
   partitions/topic vs on every machine talking to Kafka via rsyslog.

   If set, it will override any other partitioning scheme configured.

.. function::  partitions.number [positiveInteger]

   *Default: none*

   If set, specifies how many partitions exists **and** activates
   load-balancing among them. Messages are distributed more or
   less evenly between the partitions. Note that the number specified
   must be correct. Otherwise, some errors may occur or some partitions
   may never receive data.

.. function::  partitions.useFixed [positiveInteger]

   *Default: none*

   If set, specifies the partition to which data is produced. All
   data goes to this partition, no other partition is ever involved
   for this action.

.. function::  errorFile [filename]
   
   *Default: none*

   If set, messages that could not be sent and caused an error
   messages are written to the file specified. This file is in JSON
   format, with a single record being written for each message in
   error. The entry contains the full message, as well as Kafka
   error number and reason string.

   The idea behind the error file is that the admin can periodically
   run a script that reads the error file and reacts on it. Note that
   the error file is kept open from when the first error occured up
   until rsyslog is terminated or received a HUP signal.

.. function::  confParam [parameter]

   *Type:* Array

   *Default: none*

   Permits to specify Kafka options. Rather than offering a myriad of
   config settings to match the Kafka parameters, we provide this setting
   here as a vehicle to set any Kafka parameter. This has the big advantage
   that Kafka parameters that come up in new releases can immediately be used.

   Note that we use librdkafka for the Kafka connection, so the parameters
   are actually those that librdkafka supports. As of our understanding, this
   is a superset of the native Kafka parameters.

.. function::  topicConfParam [parameter]

   *Type:* Array

   *Default: none*

   In essence the same as *confParam*, but for the Kafka topic.

.. function::  template [templateName]

   *Default: template set via "template" module parameter*

   Sets the template to be used for this action.

.. function::  closeTimeout [positiveInteger]

   *Default: 2000*

   Sets the time to wait in ms (milliseconds) for draining messages submitted to kafka-handle
   (provided by librdkafka) before closing it.

   The maximum value of closeTimeout used across all omkafka action instances
   is used as librdkafka unload-timeout while unloading the module
   (for shutdown, for instance).

.. function::  resubmitOnFailure [boolean]

   *Default: off*

   *Available since: 8.28.0*

   If enabled, failed messages will be resubmit automatically when kafka is able to send 
   messages again. To prevent message loss, this option should be enabled.

.. function::  keepFailedMessages [boolean]

   *Default: off*

   *Available since: 8.28.0*

   If enabled, failed messages will be saved and loaded on shutdown/startup and resend after startup if 
   the kafka server is able to receive messages again. This setting requires resubmitOnFailure to be enabled as well.
   
.. function::  failedMsgFile [filename]

   *Default: none*

   *Available since: 8.28.0*

   Filename where the failed messages should be stored into.
   Needs to be set when keepFailedMessages is enabled, otherwise failed messages won't be saved.



Caveats/Known Bugs
------------------

-  currently none

Example
-------
To be added, see intro to action parameters.
