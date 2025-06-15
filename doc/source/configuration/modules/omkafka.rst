******************************
omkafka: write to Apache Kafka
******************************

===========================  ===========================================================================
**Module Name:**             **omkafka**
**Author:**                  `Rainer Gerhards <https://rainer.gerhards.net/>`_ <rgerhards@adiscon.com>
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


DynaKey
^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive", "Available since"
   :widths: auto
   :class: parameter-table

   "binary", "off", "no", "none", v8.1903

If set, the key parameter becomes a template for the key to base the
partitioning on. 


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


Partitions.number
^^^^^^^^^^^^^^^^^

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
the error file is kept open from when the first error occurred up
until rsyslog is terminated or received a HUP signal.


statsFile
^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "none", "no", "none"

If set, the contents of the JSON object containing the full librdkafka
statistics will be written to the file specified. The file will be
updated based on the statistics.interval.ms confparam value, which must
also be set.


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

**Note:** Messages that are rejected by kafka due to exceeding the maximum configured
message size, are automatically dropped. These errors are not retriable.

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


statsName
^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "none", "no", "none"

.. versionadded:: 8.2108.0

The name assigned to statistics specific to this action instance. The supported set of
statistics tracked for this action instance are **submitted**, **acked**, **failures**.
See the :ref:`statistics-counter_label` section for more details.


.. _statistics-counter_label:

Statistic Counter
=================

This plugin maintains global :doc:`statistics <../rsyslog_statistic_counter>` for omkafka that
accumulate all action instances. The statistic origin is named "omafka" with following counters:

- **submitted** - number of messages submitted to omkafka for processing (with both acknowledged
  deliveries to broker as well as failed or re-submitted from omkafka to librdkafka).

- **maxoutqsize** - high water mark of output queue size.

- **failures** - number of messages that librdkafka failed to deliver. This number is
  broken down into counts of various types of failures.

- **topicdynacache.skipped** - count of dynamic topic cache lookups that find an existing topic and
  skip creating a new one.

- **topicdynacache.miss** - count of dynamic topic cache lookups that fail to find an existing topic
  and end up creating new ones.

- **topicdynacache.evicted** - count of dynamic topic cache entry evictions.

- **acked** - count of messages that were acknowledged by kafka broker. Note that
  kafka broker provides two levels of delivery acknowledgements depending on topicConfParam:
  default (acks=1) implies delivery to the leader only while acks=-1 implies delivery to leader
  as well as replication to all brokers.

- **failures_msg_too_large** - count of messages dropped by librdkafka when it failed to
  deliver to the broker because broker considers message to be too large. Note that
  omkafka may still resubmit to librdkafka depending on resubmitOnFailure option.

- **failures_unknown_topic** - count of messages dropped by librdkafka when it failed to
  deliver to the broker because broker does not recognize the topic.

- **failures_queue_full** - count of messages dropped by librdkafka when its queue becomes
  full. Note that default size of librdkafka queue is 100,000 messages.

- **failures_unknown_partition** - count of messages that librdkafka failed to deliver because
  broker does not recognize a partition.

- **failures_other** - count of all of the rest of the failures that do not fall in any of
  the above failure categories.

- **errors_timed_out** - count of messages that librdkafka could not deliver within timeout. These
  errors will cause action to be suspended but messages can be retried depending on retry options.

- **errors_transport** - count of messages that librdkafka could not deliver due to transport errors.
  These messages can be retried depending on retry options.

- **errors_broker_down** - count of messages that librdkafka could not deliver because it thinks that
  broker is not accessible. These messages can be retried depending on options.

- **errors_auth** - count of messages that librdkafka could not deliver due to authentication errors.
  These messages can be retried depending on the options.

- **errors_ssl** - count of messages that librdkafka could not deliver due to ssl errors.
  These messages can be retried depending on the options.

- **errors_other** - count of rest of librdkafka errors.

- **rtt_avg_usec** - broker round trip time in microseconds averaged over all brokers. It is based
  on the statistics callback window specified through statistics.interval.ms parameter to librdkafka.
  Average exclude brokers with less than 100 microseconds rtt.

- **throttle_avg_msec** - broker throttling time in milliseconds averaged over all brokers. This is
  also a part of window statistics delivered by librdkakfka. Average excludes brokers with zero throttling time.

- **int_latency_avg_usec** - internal librdkafka producer queue latency in microseconds averaged other
  all brokers. This is also part of window statistics and average excludes brokers with zero internal latency.

Note that three window statics counters are not safe with multiple clients. When statistics callback is
enabled, for example, by using statics.callback.ms=60000, omkafka will generate an internal log message every
minute for the corresponding omkafka action:

.. code-block:: none

	2018-03-31T01:51:59.368491+00:00 app1-1.example.com rsyslogd: statscb_window_stats:
	handler_name=collections.rsyslog.core#producer-1 replyq=0 msg_cnt=30 msg_size=37986 msg_max=100000
	msg_size_max=1073741824 rtt_avg_usec=41475 throttle_avg_msec=0 int_latency_avg_usec=2943224 [v8.32.0]

For multiple actions using statistics callback, there will be one such record for each action after specified
window period. See https://github.com/edenhill/librdkafka/wiki/Statistics for more details on statistics
callback values.

Examples
========

.. _omkafka-examples-label:

Using Array Type Parameter
--------------------------

Set a single value
^^^^^^^^^^^^^^^^^^

For example, to select "snappy" compression, you can use:

.. code-block:: none

   action(type="omkafka" topic="mytopic" confParam="compression.codec=snappy")


which is equivalent to:

.. code-block:: none

   action(type="omkafka" topic="mytopic" confParam=["compression.codec=snappy"])


Set multiple values
^^^^^^^^^^^^^^^^^^^

To specify multiple values, just use the bracket notation and create a
comma-delimited list of values as shown here:

.. code-block:: none

   action(type="omkafka" topic="mytopic"
          confParam=["compression.codec=snappy",
	             "socket.timeout.ms=5",
		     "socket.keepalive.enable=true"]
         )


