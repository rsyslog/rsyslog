.. _module-omkinesis:

.. meta::
   :description: Amazon Kinesis Data Streams PutRecord output module.
   :keywords: rsyslog, omkinesis, aws, kinesis, putrecord

.. summary-start

``omkinesis`` sends each formatted message to Amazon Kinesis Data Streams
using the PutRecord API and libcurl AWS Signature Version 4 signing.

.. summary-end

*******************************************
omkinesis: Amazon Kinesis output module
*******************************************

The module builds automatically when libcurl 7.75.0 or newer is available;
``--enable-omkinesis`` requires it explicitly. The action
requires a stream name, a region, a partition key, and credentials. Use
``partition_key`` for a fixed key or ``partition_key_template`` to derive a
key from each message. A template based key can distribute traffic across
shards. Supply
``access_key`` and ``secret_key`` together or use ``AWS_ACCESS_KEY_ID`` and
``AWS_SECRET_ACCESS_KEY`` in the daemon environment. ``AWS_SESSION_TOKEN`` is
supported for temporary credentials. Credential values are read at
configuration time; restart rsyslog to rotate them. IAM roles and AWS shared
credential files are not read by this module.

Each action call sends one record. A non-200 response or transport failure
suspends the action, so standard ``action.resumeInterval`` and
``action.resumeRetryCount`` settings control retry behavior. As with other
retriable outputs, delivery can be duplicated after an uncertain response.
The module currently limits the rendered data to 1 MiB per record and the
partition key to 256 bytes.

Configuration parameters
========================

.. toctree::
   :maxdepth: 1

   ../../reference/parameters/omkinesis-stream
   ../../reference/parameters/omkinesis-region
   ../../reference/parameters/omkinesis-partition_key
   ../../reference/parameters/omkinesis-partition_key_template
   ../../reference/parameters/omkinesis-template
   ../../reference/parameters/omkinesis-access_key
   ../../reference/parameters/omkinesis-secret_key
   ../../reference/parameters/omkinesis-session_token
   ../../reference/parameters/omkinesis-endpoint
   ../../reference/parameters/omkinesis-timeout

RainerScript example
====================

.. code-block:: rsyslog

   module(load="omkinesis")
   template(name="kinesisPayload" type="string" string="%msg%")
   template(name="kinesisKey" type="string" string="%hostname%")
   action(type="omkinesis" stream="my-stream" region="us-east-1"
          partition_key_template="kinesisKey" template="kinesisPayload"
          action.resumeRetryCount="-1")

YAML example
============

.. code-block:: yaml

   version: 2
   templates:
     - name: kinesisPayload
       type: string
       string: "%msg%"
     - name: kinesisKey
       type: string
       string: "%hostname%"
   rulesets:
     - name: main
       statements:
         - type: omkinesis
           stream: my-stream
           region: us-east-1
           partition_key_template: kinesisKey
           template: kinesisPayload

Load the module with ``module(load="omkinesis")`` in the enclosing
RainerScript configuration before including the YAML file.
