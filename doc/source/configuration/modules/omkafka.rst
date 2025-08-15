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

   Parameter names are case-insensitive; CamelCase is recommended for readability.

.. toctree::
   :hidden:

   ../../reference/parameters/omkafka-broker
   ../../reference/parameters/omkafka-topic
   ../../reference/parameters/omkafka-key
   ../../reference/parameters/omkafka-dynakey
   ../../reference/parameters/omkafka-dynatopic
   ../../reference/parameters/omkafka-dynatopic-cachesize
   ../../reference/parameters/omkafka-partitions-auto
   ../../reference/parameters/omkafka-partitions-number
   ../../reference/parameters/omkafka-partitions-usefixed
   ../../reference/parameters/omkafka-errorfile
   ../../reference/parameters/omkafka-statsfile
   ../../reference/parameters/omkafka-confparam
   ../../reference/parameters/omkafka-topicconfparam
   ../../reference/parameters/omkafka-template
   ../../reference/parameters/omkafka-closetimeout
   ../../reference/parameters/omkafka-resubmitonfailure
   ../../reference/parameters/omkafka-keepfailedmessages
   ../../reference/parameters/omkafka-failedmsgfile
   ../../reference/parameters/omkafka-statsname

Action Parameters
-----------------

.. list-table::
   :widths: 30 70
   :header-rows: 1

   * - Parameter
     - Summary
   * - :ref:`param-omkafka-broker`
     - .. include:: ../../reference/parameters/omkafka-broker.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omkafka-topic`
     - .. include:: ../../reference/parameters/omkafka-topic.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omkafka-key`
     - .. include:: ../../reference/parameters/omkafka-key.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omkafka-dynakey`
     - .. include:: ../../reference/parameters/omkafka-dynakey.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omkafka-dynatopic`
     - .. include:: ../../reference/parameters/omkafka-dynatopic.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omkafka-dynatopic-cachesize`
     - .. include:: ../../reference/parameters/omkafka-dynatopic-cachesize.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omkafka-partitions-auto`
     - .. include:: ../../reference/parameters/omkafka-partitions-auto.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omkafka-partitions-number`
     - .. include:: ../../reference/parameters/omkafka-partitions-number.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omkafka-partitions-usefixed`
     - .. include:: ../../reference/parameters/omkafka-partitions-usefixed.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omkafka-errorfile`
     - .. include:: ../../reference/parameters/omkafka-errorfile.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omkafka-statsfile`
     - .. include:: ../../reference/parameters/omkafka-statsfile.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omkafka-confparam`
     - .. include:: ../../reference/parameters/omkafka-confparam.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omkafka-topicconfparam`
     - .. include:: ../../reference/parameters/omkafka-topicconfparam.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omkafka-template`
     - .. include:: ../../reference/parameters/omkafka-template.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omkafka-closetimeout`
     - .. include:: ../../reference/parameters/omkafka-closetimeout.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omkafka-resubmitonfailure`
     - .. include:: ../../reference/parameters/omkafka-resubmitonfailure.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omkafka-keepfailedmessages`
     - .. include:: ../../reference/parameters/omkafka-keepfailedmessages.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omkafka-failedmsgfile`
     - .. include:: ../../reference/parameters/omkafka-failedmsgfile.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omkafka-statsname`
     - .. include:: ../../reference/parameters/omkafka-statsname.rst
        :start-after: .. summary-start
        :end-before: .. summary-end

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


.. _omkafka-sasl-password-from-env:

Set SASL password from an environment variable
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. versionchanged:: 8.2508.0

   Backticks in RainerScript support the ``${VAR}`` form and adjacent text.
   This enables a simpler inline configuration such as::

      `echo sasl.password=${KAFKA_PASSWORD}`

**Recommended (rsyslog v8.2508.0 and later)**

Keep the secret out of *rsyslog.conf* and inject it via environment.
Then build the ``key=value`` pair inline with backticks:

.. code-block:: bash

   # set by your service manager or a secure env file
   export KAFKA_PASSWORD='supersecret'

.. code-block:: rsyslog

   action(
     type="omkafka"
     broker=["kafka.example.com:9093"]
     confParam=[
       "security.protocol=SASL_SSL",
       "sasl.mechanism=SCRAM-SHA-512",
       "sasl.username=myuser",
       `echo sasl.password=${KAFKA_PASSWORD}`
     ]
   )

Notes:

- This relies on the enhanced backtick handling; it is **not** a general shell.
  Only the documented backtick subset (notably ``echo`` and ``cat``) is supported.
- The variable expansion happens at rsyslog parse time, using the process
  environment of the rsyslog daemon.

**Older rsyslog versions (before v8.2508.0)**

Backticks did **not** understand ``${VAR}`` or adjacency. Inline forms like
`` `echo sasl.password=$KAFKA_PASSWORD` `` could cause errors such as
“missing equal sign in parameter”. Use a pre-composed environment variable that
already contains the full ``key=value`` pair and echo **that**:

.. code-block:: bash

   export KAFKA_PASSWORD='supersecret'
   # Pre-compose the full key=value (done *outside* rsyslog)
   export SASL_PWDPARAM="sasl.password=${KAFKA_PASSWORD}"

.. code-block:: rsyslog

   action(
     type="omkafka"
     broker=["kafka.example.com:9093"]
     confParam=[
       "security.protocol=SASL_SSL",
       "sasl.mechanism=SCRAM-SHA-512",
       "sasl.username=myuser",
       `echo $SASL_PWDPARAM`
     ]
   )

Security guidance
^^^^^^^^^^^^^^^^^

- Prefer environment files or service manager mechanisms with strict permissions
  over embedding secrets directly in *rsyslog.conf*.
- Process environments may be visible to privileged users (e.g., via ``/proc``);
  secure host access accordingly.
