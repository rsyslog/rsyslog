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

.. list-table::
   :widths: 30 70
   :header-rows: 1

   * - Parameter
     - Summary
   * - :ref:`param-imkafka-ruleset`
     - .. include:: ../../reference/parameters/imkafka-ruleset.rst
        :start-after: .. summary-start
        :end-before: .. summary-end


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
   * - :ref:`param-imkafka-topic`
     - .. include:: ../../reference/parameters/imkafka-topic.rst
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
   * - :ref:`param-imkafka-ruleset`
     - .. include:: ../../reference/parameters/imkafka-ruleset.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imkafka-parsehostname`
     - .. include:: ../../reference/parameters/imkafka-parsehostname.rst
        :start-after: .. summary-start
        :end-before: .. summary-end

.. toctree::
   :hidden:

   ../../reference/parameters/imkafka-broker
   ../../reference/parameters/imkafka-topic
   ../../reference/parameters/imkafka-confparam
   ../../reference/parameters/imkafka-consumergroup
   ../../reference/parameters/imkafka-ruleset
   ../../reference/parameters/imkafka-parsehostname


Caveats/Known Bugs
==================

-  currently none


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
