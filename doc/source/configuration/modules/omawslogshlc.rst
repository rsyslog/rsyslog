.. _module-omawslogshlc:

.. meta::
   :description: Configuration reference for the omawslogshlc Amazon CloudWatch Logs output module.
   :keywords: rsyslog, omawslogshlc, aws, cloudwatch, logs, hlc

.. summary-start

``omawslogshlc`` sends log messages to Amazon CloudWatch Logs using the
HTTP Log Collector (HLC) endpoint with bearer token authentication. It is a
user-contributed, experimental module with higher operational risk than
core-supported mature modules.

.. summary-end

********************************************************
omawslogshlc: Amazon CloudWatch Logs HLC Output Module
********************************************************

===========================  ===========================================================================
**Module Name:**             **omawslogshlc**
**Author:**                  Amazon CloudWatch Logs team
**Status:**                  user-contributed, experimental
**Available since:**         v8.2604
===========================  ===========================================================================

.. warning::

   ``omawslogshlc`` is user-contributed and experimental. It has had less
   production exposure and core-maintainer ownership than mature rsyslog
   modules, so deployments should treat it as higher risk, test it carefully,
   and expect possible feedback-driven behavior or configuration changes.


Purpose
-------

This module provides native support for forwarding syslog messages to Amazon
CloudWatch Logs. It uses the publicly available HLC (HTTP Log Collector)
endpoint and bearer token authentication, eliminating the need for the AWS SDK
or a separate log collection agent.

Each syslog message is wrapped in the HLC JSON event format and posted to the
configured CloudWatch Logs log group and log stream.


Notable Features
----------------

- Bearer token authentication (no AWS SDK dependency)
- Automatic event batching with configurable batch size
- JSON escaping of message content
- Automatic inclusion of timestamp, hostname, and source metadata
- User-Agent header identifying rsyslog version
- Retry-friendly error handling for transient failures (429, 5xx)


Requirements
------------

To use ``omawslogshlc``, you need the following:

- ``libcurl`` support at build time
- An AWS region where CloudWatch Logs is available
- A CloudWatch Logs log group with bearer token authentication enabled
- A bearer token generated for that log group (starts with ``ACWL``)
- A log stream within the log group

The module is built only when ``./configure`` is invoked with
``--enable-omawslogshlc``.

For information on setting up bearer token authentication, see the
`AWS documentation <https://docs.aws.amazon.com/AmazonCloudWatch/latest/logs/CWL_HLC_Endpoint.html>`_.


Configuration Parameters
------------------------

.. note::

   Parameter names are case-insensitive.

.. note::

   This module supports action parameters only.

Action Parameters
~~~~~~~~~~~~~~~~~

region
^^^^^^

.. list-table::
   :widths: 20 80

   * - **Type:**
     - string
   * - **Required:**
     - yes

The AWS region for the CloudWatch Logs endpoint (e.g., ``us-west-2``,
``eu-west-1``). This is used to construct the HLC endpoint URL:
``https://logs.<region>.amazonaws.com/services/collector/event``

bearer_token
^^^^^^^^^^^^

.. list-table::
   :widths: 20 80

   * - **Type:**
     - string
   * - **Required:**
     - yes

The bearer token for HLC endpoint authentication. Tokens start with ``ACWL``.
This value is sent in the ``Authorization: Bearer <token>`` header.

log_group
^^^^^^^^^

.. list-table::
   :widths: 20 80

   * - **Type:**
     - string
   * - **Required:**
     - yes

The name of the CloudWatch Logs log group to send events to. The value is
URL-encoded automatically.

log_stream
^^^^^^^^^^

.. list-table::
   :widths: 20 80

   * - **Type:**
     - string
   * - **Required:**
     - yes

The name of the CloudWatch Logs log stream within the log group. The value is
URL-encoded automatically.

max_batch_size
^^^^^^^^^^^^^^

.. list-table::
   :widths: 20 80

   * - **Type:**
     - integer
   * - **Default:**
     - 100
   * - **Required:**
     - no

Maximum number of events per HTTP request. When the batch reaches this count,
it is flushed immediately. Valid range is 1 to 10000. AWS recommends 10–100
events per request for optimal performance.

template
^^^^^^^^

.. list-table::
   :widths: 20 80

   * - **Type:**
     - word
   * - **Default:**
     - RSYSLOG_TraditionalFileFormat
   * - **Required:**
     - no

The rsyslog template used to render the message content. The rendered output
becomes the ``event`` field in the HLC JSON payload.

.. toctree::
   :hidden:

   ../../reference/parameters/omawslogshlc-region
   ../../reference/parameters/omawslogshlc-bearer_token
   ../../reference/parameters/omawslogshlc-log_group
   ../../reference/parameters/omawslogshlc-log_stream
   ../../reference/parameters/omawslogshlc-max_batch_size
   ../../reference/parameters/omawslogshlc-template


Batching Behavior
-----------------

``omawslogshlc`` accumulates events within a transaction and flushes them
when one of the following happens:

- The event count reaches ``max_batch_size``
- The rsyslog transaction ends (``endTransaction``)
- The total batch size approaches the 1 MiB HLC request limit

Events are sent as concatenated JSON objects (no array wrapper), which is one
of the accepted formats for the HLC endpoint.


Error Handling
--------------

The module handles HTTP errors as follows:

- **200–299**: Success. Batch is cleared.
- **401/403**: Authentication failure. The action is suspended for retry.
  Check that the bearer token is valid and not expired.
- **429**: Rate limited. The action is suspended and retried according to
  rsyslog's action retry settings.
- **5xx**: Server error. The action is suspended for retry.
- **Other 4xx**: Non-retryable. The batch is dropped and an error is logged.

Use rsyslog's ``action.resumeInterval`` and ``action.resumeRetryCount`` to
control retry behavior.


Example
-------

Forward all syslog messages to CloudWatch Logs:

.. code-block:: none

   module(load="omawslogshlc")

   *.* action(
      type="omawslogshlc"
      region="us-west-2"
      bearer_token="ACWL..."
      log_group="my-application-logs"
      log_stream="server-01"
   )

Forward only auth logs with a larger batch size:

.. code-block:: none

   module(load="omawslogshlc")

   auth,authpriv.* action(
      type="omawslogshlc"
      region="eu-west-1"
      bearer_token="ACWL..."
      log_group="security-logs"
      log_stream="auth"
      max_batch_size="50"
      action.resumeInterval="5"
      action.resumeRetryCount="10"
   )

YAML configuration for the same target:

.. code-block:: yaml

   version: 2
   modules:
     - load: omawslogshlc

   rulesets:
     - name: main
       filter: "*.*"
       actions:
         - type: omawslogshlc
           region: us-west-2
           bearer_token: ACWL...
           log_group: my-application-logs
           log_stream: server-01
           max_batch_size: 50
