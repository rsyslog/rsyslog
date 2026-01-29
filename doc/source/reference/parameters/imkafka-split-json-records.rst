.. _param-imkafka-split-json-records:
.. _imkafka.parameter.input.split.json.records:

split.json.records
==================

.. index::
   single: imkafka; split.json.records
   single: split.json.records

.. summary-start

Controls whether imkafka splits JSON batches into individual messages.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imkafka`.

:Name: split.json.records
:Scope: input
:Type: boolean
:Default: input=``off``
:Required?: no
:Introduced: 8.2502.0

Description
-----------
When enabled, imkafka will detect JSON messages with the format ``{"records":[...]}`` 
and split the array into individual syslog messages. Each record within the array 
becomes a separate message, allowing for message-level filtering and processing.

This is particularly useful when consuming messages from Azure Event Hub or other 
systems that batch multiple log records into a single Kafka message.

**Behavior:**

- If splitting is enabled and the message contains a ``"records"`` array, each 
  element is submitted as a separate message.
- If the ``"records"`` field is not found or the JSON is malformed, the entire 
  message is forwarded unchanged.
- When a record contains a ``"time"`` field (ISO 8601 timestamp), it is used as 
  the message timestamp.
- Empty arrays ``{"records":[]}`` are forwarded as a single message without splitting.

**Error Handling:**

- Malformed JSON: A warning is logged and the message is forwarded as received.
- Missing ``"records"`` field: The message is processed as a normal single event.

Input usage
-----------
.. _imkafka.parameter.input.split.json.records-usage:

.. code-block:: rsyslog

   module(load="imkafka")
   input(type="imkafka"
         topic="your-topic"
         broker="localhost:9092"
         consumergroup="default"
         split.json.records="on")

Example
-------

**Input (Single Kafka Message):**

.. code-block:: json

   {
     "records": [
       {
         "time": "2025-02-20T03:19:34.655Z",
         "operationName": "CreatePathDir",
         "statusCode": 409
       },
       {
         "time": "2025-02-20T03:19:34.693Z",
         "operationName": "CreatePathDir",
         "statusCode": 409
       }
     ]
   }

**Output (Two Separate Syslog Messages):**

.. code-block:: json

   {"time":"2025-02-20T03:19:34.655Z","operationName":"CreatePathDir","statusCode":409}

.. code-block:: json

   {"time":"2025-02-20T03:19:34.693Z","operationName":"CreatePathDir","statusCode":409}

See also
--------
See also :doc:`../../configuration/modules/imkafka`.
