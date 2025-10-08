*******************************
imkafka: read from Apache Kafka
*******************************

===========================  ===========================================================================
**Module Name:**             **imkafka**
**Author:**                  Andre Lorbach <alorbach@adiscon.com>
**Available since:**         8.27.0
===========================  ===========================================================================


Purpose
=======

The imkafka plug-in implements an Apache Kafka consumer, permitting
rsyslog to receive data from Kafka.


Configuration Parameters
========================

Note that imkafka supports some *Array*-type parameters. While the parameter
name can only be set once, it is possible to set multiple values with that
single parameter.

For example, to select a broker, you can use

.. code-block:: none

   input(type="imkafka" topic="mytopic" broker="localhost:9092" consumergroup="default")

which is equivalent to

.. code-block:: none

   input(type="imkafka" topic="mytopic" broker=["localhost:9092"] consumergroup="default")

To specify multiple values, just use the bracket notation and create a
comma-delimited list of values as shown here:

.. code-block:: none

   input(type="imkafka" topic="mytopic"
          broker=["localhost:9092",
                  "localhost:9093",
                  "localhost:9094"]
         )


.. note::

   Parameter names are case-insensitive; camelCase is recommended for readability.


Module Parameters
-----------------

Currently none.


Input Parameters
----------------

.. list-table::
   :widths: 30 70
   :header-rows: 1

   * - Parameter
     - Summary
   * - :ref:`param-imkafka-broker`
     - .. include:: ../../reference/parameters/imkafka-broker.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imkafka-confparam`
     - .. include:: ../../reference/parameters/imkafka-confparam.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imkafka-consumergroup`
     - .. include:: ../../reference/parameters/imkafka-consumergroup.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imkafka-parsehostname`
     - .. include:: ../../reference/parameters/imkafka-parsehostname.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imkafka-ruleset`
     - .. include:: ../../reference/parameters/imkafka-ruleset.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imkafka-topic`
     - .. include:: ../../reference/parameters/imkafka-topic.rst
        :start-after: .. summary-start
        :end-before: .. summary-end

.. toctree::
   :hidden:

   ../../reference/parameters/imkafka-broker
   ../../reference/parameters/imkafka-confparam
   ../../reference/parameters/imkafka-consumergroup
   ../../reference/parameters/imkafka-parsehostname
   ../../reference/parameters/imkafka-ruleset
   ../../reference/parameters/imkafka-topic

Statistic Counters
==================
Explanation
-----------
These fields come from rsyslog’s periodic statistics (`impstats`) for the imkafka input module (Kafka consumer built on - **librdkafka- **). `impstats` emits per-component counters as JSON or legacy text at a fixed interval; each stats object has a name and origin (the component that registered the counters).
Metrics are tracked at the global (module) level which exposes additional aggregate data from `librdkafka` metrics. They're also tracked at the local `action` level for more fine-grained tracking in evnrionments with many data pipelines. Metrics remain compatible with most pstats formats, but the `zabbix` format is recommended for
systems utilizing low-level discovery and JSON compatibility.

Definition list (Global Stats)
------------------------------
- **name** - Identifier of the statistics object. For imkafka’s *global* counters this is usually `"imkafka"` (or an instance-specific label if configured).
- **origin** - The module that registered the counters; for these metrics it is `"imkafka"`. `origin` indicates the source subsystem in rsyslog’s statistics framework.
- **received** - Total number of Kafka records the imkafka consumer fetched from its assigned partitions since rsyslog start (or since the last counter reset).
- **submitted** - Total number of messages the input submitted into rsyslog’s processing pipeline. On a healthy pipeline this closely matches `received`.
- **failures** - Count of messages imkafka could not deliver into rsyslog’s pipeline (e.g., due to internal errors).
- **eof** - Number of end-of-partition events seen (Kafka error `RD_KAFKA_RESP_ERR__PARTITION_EOF`). These occur when the consumer reaches the current end of a partition.
- **poll_empty** - Number of consumer poll cycles that returned no messages (topic empty or no fetchable data at that moment). This can be normal but can also indicate that a producer is not sending logs to Kafka.
- **maxlag** - Maximum consumer lag observed (in messages) across the consumer’s assigned partitions. A rising maxlog value indicates that consumers are falling behind the producers. Consider adding additional consumers (and/or partitions)
- **rtt_avg_usec** - Average broker round-trip time (RTT) in microseconds, aggregated from librdkafka’s broker-level metrics.
- **throttle_avg_msec** - Average broker-imposed throttle time in milliseconds (from broker quota throttling).
- **int_latency_avg_usec** - Average internal request latency in microseconds within the Kafka client.
- **errors_timed_out** - Count of Kafka operations that timed out (e.g., fetch or metadata requests).
- **errors_transport** - Count of transport-level failures such as connect/reset errors at the TCP layer. Check firewall policies on port 9092, 9093 (default).
- **errors_broker_down** - Count of conditions where your brokers are unavailable. Check firewall policies on port 9092, 9093 (default). Ensure your Kafka services are running.
- **errors_auth** - Count of authentication failures (e.g., SASL mechanism/credential problems).
- **errors_ssl** - Count of SSL/TLS  handshake/validation failures. Check that certificates are correct and not being intercepted/decrypted.
- **errors_other** - Catch-all for other librdkafka/consumer errors not categorized above.

Definition list (Local Stats)
-----------------------------
- **name** Identifier for the action-level statistics object. It includes the module name and the topic_consumer group pair in brackets. (eg: "name": "imkafka[topic_consumergroup]")
- **origin** The module that registered the counters; for these metrics it is always `"imkafka"`.
- **received** Number of Kafka records fetched for this specific topic and consumer group since rsyslog start (or last reset).
- **submitted** Number of messages successfully submitted into rsyslog’s processing pipeline for this topic and consumer group.
- **failures** Count of messages that could not be delivered into rsyslog’s pipeline for this topic/consumer group.
- **eof** Number of **end-of-partition** events for this topic/consumer group (Kafka error `RD_KAFKA_RESP_ERR__PARTITION_EOF`).
- **poll_empty** Number of poll cycles for this topic/consumer group that returned no messages.
- **maxlag** Maximum **consumer lag** observed (in messages) for this topic/consumer group. Indicates how far behind the consumer is compared to the partition head.

Caveats/Known Bugs
==================

-  currently none

Notes
=====
- Metrics have been implemented with the help of JSON parsers for librdkafka emissions.
- When `format=zabbix` is specified in impstats, local metrics are broken out from other `core.action` values.


Examples
========

Example 1
---------

In this sample a consumer for the topic static is created and will forward the messages to the omfile action.

.. code-block:: none

   module(load="imkafka")
   input(type="imkafka" topic="static" broker="localhost:9092"
                        consumergroup="default" ruleset="pRuleset")

   ruleset(name="pRuleset") {
   	action(type="omfile" file="path/to/file")
   }
