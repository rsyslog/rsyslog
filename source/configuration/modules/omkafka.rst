******************************
omkafka: write to Apache Kafka
******************************

===========================  ===========================================================================
**Module Name:**             **omkafka**
**Author:**                  `Rainer Gerhards <http://rainer.gerhards.net/>`_ <rgerhards@adiscon.com>
**Available since:**         v8.7.0
===========================  ===========================================================================


Purpose
=======

The omkafka plug-in implements an Apache Kafka producer, permitting
rsyslog to write data to Kafka.


Configuration Parameters
========================

.. note::

   Parameter names are case-insensitive.


Action Parameters
-----------------

Note that omkafka supports some *Array*-type parameters. While the parameter
name can only be set once, it is possible to set multiple values with that
single parameter. See the :ref:`omkafka-examples-label` section for details.


Broker
^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "array", "localhost:9092", "no", "none"

Specifies the broker(s) to use.


Topic
^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "string", "none", "yes", "none"

Specifies the topic to produce to.


Key
^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "none", "no", "none"

Kafka key to be used for all messages.

If a key is provided and partitions.auto="on" is set, then all messages will
be assigned to a partition based on the key.


DynaTopic
^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "binary", "off", "no", "none"

If set, the topic parameter becomes a template for which topic to
produce messages to. The cache is cleared on HUP.


DynaTopic.Cachesize
^^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "50", "no", "none"

If set, defines the number of topics that will be kept in the dynatopic
cache.


Partitions.Auto
^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "binary", "off", "no", "none"

Librdkafka provides an automatic partitioning function that will
automatically distribute the produced messages into all partitions
configured for that topic.

To use, set partitions.auto="on". This is instead of specifying the
number of partitions on the producer side, where it would be easier
to change the kafka configuration on the cluster for number of
partitions/topic vs on every machine talking to Kafka via rsyslog.

If no key is set, messages will be distributed randomly across partitions.
This results in a very even load on all partitions, but does not preserve
ordering between the messages.

If a key is set, a partition will be chosen automatically based on it. All
messages with the same key will be sorted into the same partition,
preserving their ordering. For example, by setting the key to the hostname,
messages from a specific host will be written to one partition and ordered,
but messages from different nodes will be distributed across different
partitions. This distribution is essentially random, but stable. If the
number of different keys is much larger than the number of partitions on the
topic, load will be distributed fairly evenly.

If set, it will override any other partitioning scheme configured.


Partitons.number
^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "none", "no", "none"

If set, specifies how many partitions exists **and** activates
load-balancing among them. Messages are distributed more or
less evenly between the partitions. Note that the number specified
must be correct. Otherwise, some errors may occur or some partitions
may never receive data.


Partitions.useFixed
^^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "none", "no", "none"

If set, specifies the partition to which data is produced. All
data goes to this partition, no other partition is ever involved
for this action.


errorFile
^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "none", "no", "none"

If set, messages that could not be sent and caused an error
messages are written to the file specified. This file is in JSON
format, with a single record being written for each message in
error. The entry contains the full message, as well as Kafka
error number and reason string.

The idea behind the error file is that the admin can periodically
run a script that reads the error file and reacts on it. Note that
the error file is kept open from when the first error occured up
until rsyslog is terminated or received a HUP signal.


ConfParam
^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "array", "none", "no", "none"

Permits to specify Kafka options. Rather than offering a myriad of
config settings to match the Kafka parameters, we provide this setting
here as a vehicle to set any Kafka parameter. This has the big advantage
that Kafka parameters that come up in new releases can immediately be used.

Note that we use librdkafka for the Kafka connection, so the parameters
are actually those that librdkafka supports. As of our understanding, this
is a superset of the native Kafka parameters.


TopicConfParam
^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "array", "none", "no", "none"

In essence the same as *confParam*, but for the Kafka topic.


Template
^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "template set via template module parameter", "no", "none"

Sets the template to be used for this action.


closeTimeout
^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "2000", "no", "none"

Sets the time to wait in ms (milliseconds) for draining messages submitted to kafka-handle
(provided by librdkafka) before closing it.

The maximum value of closeTimeout used across all omkafka action instances
is used as librdkafka unload-timeout while unloading the module
(for shutdown, for instance).


resubmitOnFailure
^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "binary", "off", "no", "none"

.. versionadded:: 8.28.0

If enabled, failed messages will be resubmit automatically when kafka is able to send
messages again. To prevent message loss, this option should be enabled.


KeepFailedMessages
^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "binary", "off", "no", "none"

If enabled, failed messages will be saved and loaded on shutdown/startup and resend after startup if
the kafka server is able to receive messages again. This setting requires resubmitOnFailure to be enabled as well.


failedMsgFile
^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "none", "no", "none"

.. versionadded:: 8.28.0

Filename where the failed messages should be stored into.
Needs to be set when keepFailedMessages is enabled, otherwise failed messages won't be saved.


Examples
========

.. _omkafka-examples-label:


Set a single value
------------------

For example, to select "snappy" compression, you can use:

.. code-block:: none

   action(type="omkafka" topic="mytopic" confParam="compression.codec=snappy")


which is equivalent to:

.. code-block:: none

   action(type="omkafka" topic="mytopic" confParam=["compression.codec=snappy"])


Set multiple values
-------------------

To specify multiple values, just use the bracket notation and create a
comma-delimited list of values as shown here:

.. code-block:: none

   action(type="omkafka" topic="mytopic"
          confParam=["compression.codec=snappy",
	             "socket.timeout.ms=5",
		     "socket.keepalive.enable=true"]
         )


