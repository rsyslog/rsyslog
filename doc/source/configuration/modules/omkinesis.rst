.. SPDX-License-Identifier: Apache-2.0

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

Each action call sends one record. Transport failures, HTTP 429 and 5xx
responses, and HTTP 400 responses containing
``ProvisionedThroughputExceededException``, ``InternalFailure``, or
``KMSThrottlingException`` suspend the action. Standard
``action.resumeInterval`` and ``action.resumeRetryCount`` settings control
these retries. Other non-200 responses are not retried. Delivery can be
duplicated after an uncertain response. The partition key is limited to 256
bytes. The default combined data and partition key limit is 1 MiB; set
``max_record_size`` up to 10 MiB only after enabling larger records on the
Kinesis stream.

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
   ../../reference/parameters/omkinesis-max_record_size

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
