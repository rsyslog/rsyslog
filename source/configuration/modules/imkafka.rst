imkafka: read from Apache Kafka
===============================

===========================  =======================================================
**Module Name:**             **imkafka**
**Author:**                  Pascal Withopf <pascalwithopf1@gmail.com>
**Available since:**         v8.27.0
===========================  =======================================================

The imkafka plug-in implements an Apache Kafka consumer, permitting
rsyslog to receive data from Kafka.

Configuration Parameters
------------------------
Note that imkafka supports some *Array*-type parameters. While the parameter
name can only be set once, it is possible to set multiple values with that
single parameter.

For example, to select a broker, you can use

::

   input(type="imkafka" topic="mytopic" broker="localhost:9092" consumergroup="default")

which is equivalent to

::

   input(type="imkafka" topic="mytopic" broker=["localhost:9092"] consumergroup="default")

To specify multiple values, just use the bracket notation and create a
comma-delimited list of values as shown here:

::

   input(type="imkafka" topic="mytopic"
          broker=["localhost:9092",
                  "localhost:9093",
                  "localhost:9094"]
         )

Module Parameters
^^^^^^^^^^^^^^^^^
Currently none.


Action Parameters
^^^^^^^^^^^^^^^^^
.. function::  broker <Array>

   *Default: "localhost:9092"*

   Specifies the broker(s) to use.

.. function::  topic <String>

   *Mandatory*

   *Default: none*

   Specifies the topic to produce to.

.. function::  confParam <Array>

   *Default: none*

   Permits to specify Kafka options. Rather than offering a myriad of
   config settings to match the Kafka parameters, we provide this setting
   here as a vehicle to set any Kafka parameter. This has the big advantage
   that Kafka parameters that come up in new releases can immediately be used.

   Note that we use librdkafka for the Kafka connection, so the parameters
   are actually those that librdkafka supports. As of our understanding, this
   is a superset of the native Kafka parameters.

.. function:: consumergroup <String>

   *Default none*

   With this parameter the group.id for the consumer is set. All consumers
   sharing the same group.id belong to the same group.

.. function:: ruleset <String>

   *Default: none*

   Specifies the ruleset to be used.

Caveats/Known Bugs
------------------

-  currently none

Example
-------

**Sample 1:**

In this sample a consumer for the topic static is created and will forward the messages to the omfile action.

::

  module(load="imkafka")
  input(type="imkafka" topic="static" broker="localhost:9092"
                       consumergroup="default" ruleset="pRuleset")

  ruleset(name="pRuleset") {
  	action(type="omfile" file="path/to/file")
  }
