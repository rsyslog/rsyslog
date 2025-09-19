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

   Parameter names are case-insensitive.


Module Parameters
-----------------

Currently none.


Action Parameters
-----------------

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


ConsumerGroup
^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "string", "none", "no", "none"

With this parameter the group.id for the consumer is set. All consumers
sharing the same group.id belong to the same group.


Ruleset
^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "string", "none", "no", "none"

Specifies the ruleset to be used.


ParseHostname
^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "binary", "off", "no", "none"

.. versionadded:: 8.38.0

If this parameter is set to on, imkafka will parse the hostname in log
if it exists. The result can be retrieved from $hostname. If it's off,
for compatibility reasons, the local hostname is used, same as the previous
version.


Caveats/Known Bugs
==================

-  currently none

Notes
=====
- Metrics have been implemented with the help of JSON parsers for librdkafka emissions.


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
