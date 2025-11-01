.. _param-omhttp-batch-format:
.. _omhttp.parameter.module.batch-format:

batch.format
============

.. index::
   single: omhttp; batch.format
   single: batch.format

.. summary-start

Chooses how omhttp combines multiple messages when batching is enabled.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omhttp`.

:Name: batch.format
:Scope: module
:Type: word
:Default: module=newline
:Required?: no
:Introduced: Not specified

Description
-----------
This parameter specifies how to combine multiple messages into a single batch. Valid options are *newline* (default), *jsonarray*, *kafkarest*, and *lokirest*.

Each message on the "Inputs" line is the templated log line that is fed into the omhttp action, and the "Output" line describes the resulting payload sent to the configured HTTP server.

1. *newline* - Concatenates each message into a single string joined by newline (``"\\n"``) characters. This mode is default and places no restrictions on the structure of the input messages.

.. code-block:: text

    Inputs: "message 1" "message 2" "message 3"
    Output: "message 1\nmessage2\nmessage3"

2. *jsonarray* - Builds a JSON array containing all messages in the batch. This mode requires that each message is parsable JSON, since the plugin parses each message as JSON while building the array.

.. code-block:: text

    Inputs: {"msg": "message 1"} {"msg": "message 2"} {"msg": "message 3"}
    Output: [{"msg": "message 1"}, {"msg": "message 2"}, {"msg": "message 3"}]

3. *kafkarest* - Builds a JSON object that conforms to the `Kafka Rest Proxy specification <https://docs.confluent.io/platform/current/kafka-rest/quickstart.html>`_. This mode requires that each message is parsable JSON, since the plugin parses each message as JSON while building the batch object.

.. code-block:: text

    Inputs: {"msg": "message 1"} {"msg": "message 2"} {"msg": "message 3"}
    Output: {"records": [{"value": {"msg": "message 1"}}, {"value": {"msg": "message 2"}}, {"value": {"msg": "message 3"}}]}

4. *lokirest* - Builds a JSON object that conforms to the `Loki Rest specification <https://github.com/grafana/loki/blob/main/docs/sources/reference/loki-http-api.md#ingest-logs>`_. This mode requires that each message is parsable JSON, since the plugin parses each message as JSON while building the batch object. Additionally, the operator is responsible for providing index keys, and message values.

.. code-block:: text

    Inputs: {"stream": {"tag1":"value1"}, values:[[ "%timestamp%", "message 1" ]]} {"stream": {"tag2":"value2"}, values:[[ %timestamp%, "message 2" ]]}
    Output: {"streams": [{"stream": {"tag1":"value1"}, values:[[ "%timestamp%", "message 1" ]]},{"stream": {"tag2":"value2"}, values:[[ %timestamp%, "message 2" ]]}]}

Module usage
------------
.. _param-omhttp-module-batch-format:
.. _omhttp.parameter.module.batch-format-usage:

.. code-block:: rsyslog

   module(load="omhttp")

   action(
       type="omhttp"
       batch="on"
       batch.format="jsonarray"
   )

See also
--------
See also :doc:`../../configuration/modules/omhttp`.
